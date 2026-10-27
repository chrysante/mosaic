#include <cassert>
#include <functional>
#include <algorithm>
#include <optional>
#include <vector>
#include <type_traits>
#include <memory>
#include <ranges>

/*
 * @brief Represents a reversible operation on a document.
 *
 * A transaction describes a state transition that can be applied to a
 * document, undone, and optionally merged with a subsequent transaction.
 *
 * Implementations must satisfy the following contracts:
 *
 *  - Applying a transaction performs its forward state transition.
 *  - Undoing a transaction reverses its effect, restoring the document to
 *    the state it had immediately before the transaction was applied.
 *  - Applying and undoing a transaction must not throw. Violating this
 *    requirement results in program termination.
 *  - Transactions must retain sufficient state to undo their effects after
 *    application, including after being merged with other transactions.
 *
 *  Transactions are expected to be used in a linear history: a transaction
 *  is applied, may be merged with subsequent applied transactions, and is
 *  eventually undone or retained in the history. Applying or undoing a
 *  transaction multiple times is only valid when explicitly supported by
 *  the implementation.
 */
template <typename Doc>
class Transaction {
public:
    virtual ~Transaction() = default;
    
    /**
     *  @brief Applies this transaction to the document.
     *
     *  Transitions the document from the state preceding this transaction
     *  to the state resulting from this transaction.
     *
     *  @param doc The document to modify.
     *
     *  @note Must not throw.
     */
    virtual void apply(Doc& document) const noexcept = 0;
    
    /** @brief Reverses the effect of this transaction.
     *
     *  Restores the document to the state it had immediately before this
     *  transaction was applied.
     *
     *  The transaction must retain all information necessary to perform
     *  this reversal. If the transaction has been merged with subsequent
     *  transactions, undo must reverse the entire merged state transition.
     *
     *  @param doc The document to restore.
     *
     *  @note Must not throw.
     */
    virtual void undo(Doc& document) const noexcept = 0;
    
    /** @brief Attempts to merge a subsequent transaction into this one.
     *
     *  The incoming transaction @p next has already been applied to the
     *  document before this function is called.
     *
     *  If merging succeeds, this transaction must represent the combined
     *  state transition of both transactions: applying it once must produce
     *  the same resulting document state as applying the original
     *  transaction followed by @p next, and undoing it must restore the
     *  document to the state preceding the original transaction.
     *
     *  If merging fails, this transaction must remain unchanged. The
     *  incoming transaction must also remain valid and must not be applied
     *  again by the caller.
     *
     *  Implementations may only merge transactions when their combined
     *  behavior can be represented correctly by this transaction.
     *
     *  @param next The subsequent transaction, already applied to the
     *              document.
     *  @return true if the transactions were successfully merged; false
     *          otherwise.
     *
     *  @note Must not throw.
     */
    virtual bool merge_with(Transaction const& next) noexcept = 0;
    
    /** @brief Returns an identifier for the concrete transaction type.
     *
     *  The returned identifier may be used to determine whether two
     *  transactions have the same concrete type before attempting a
     *  type-specific operation such as merging.
     *
     *  Implementations that provide an identifier must return the same
     *  identifier for all instances of the same concrete transaction type,
     *  and different identifiers for distinct transaction types that
     *  cannot safely be treated as the same type.
     *
     *  The identifier is non-owning and must remain valid for as long as
     *  it may be used. The default implementation returns nullptr,
     *  indicating that no type identifier is provided.
     *
     *  @return A stable, non-owning type identifier, or nullptr.
     */
    virtual void const* get_type_id() const noexcept { return nullptr; }
};

template <typename Doc>
class HistoryManager {
public:
    explicit HistoryManager(std::shared_ptr<Doc> document):
    m_document(std::move(document))
    {
        assert(m_document.get() != nullptr);
    }
    
    void push(std::unique_ptr<Transaction<Doc>> transaction) {
        m_transactions.erase(m_transactions.begin() + m_undo_pos,
                             m_transactions.end());
        m_transactions.push_back(std::move(transaction));
        ++m_undo_pos;
    }
    
    bool can_undo() const {
        return m_undo_pos != 0;
    }
    
    bool undo() {
        if (!can_undo()) return false;
        --m_undo_pos;
        m_transactions[m_undo_pos]->undo(*m_document);
        return true;
    }
    
    bool can_redo() const {
        return m_undo_pos != m_transactions.size();
    }
    
    bool redo() {
        if (!can_redo()) return false;
        m_transactions[m_undo_pos]->apply(*m_document);
        ++m_undo_pos;
        return true;
    }
    
private:
    std::shared_ptr<Doc> m_document;
    std::size_t m_undo_pos = 0;
    std::vector<std::unique_ptr<Transaction<Doc>>> m_transactions;
};

namespace internal {

template <typename T>
struct MemberFunctionTraits;

template <typename R, typename C, typename Arg>
struct MemberFunctionTraits<R (C::*)(Arg)> {
    using ArgType = Arg;
};

template <typename R, typename C, typename Arg>
struct MemberFunctionTraits<R (C::*)(Arg) const> {
    using ArgType = Arg;
};

template <typename State>
struct StateTransactionTraits {
private:
    using DocArgType = typename MemberFunctionTraits<
    decltype(&State::apply)
    >::ArgType;
    
public:
    using DocumentType = std::remove_cvref_t<DocArgType>;
};

template <typename Doc, typename State>
struct StateTransactionImpl: Transaction<Doc> {
    constexpr static void const* s_type_id = &s_type_id;
    
    void const* get_type_id() const noexcept final { return s_type_id; }
    
    void apply(Doc& doc) const noexcept final { state.apply(doc); }
    
    void undo(Doc& doc) const noexcept final { state.undo(doc); }
    
    bool merge_with(Transaction<Doc> const& next) noexcept final {
        constexpr bool have_merge_with = requires(State& state, State const& next) {
            state.merge_with(next);
        };
        if constexpr (have_merge_with) {
            if (get_type_id() != next.get_type_id())
                return false;
            auto* next_derived = static_cast<StateTransactionImpl const*>(&next);
            return state.merge_with(next_derived->state);
        }
        else {
            return false;
        }
    }
    
    StateTransactionImpl(State&& state): state(std::move(state)) {}
    State state;
};

} // namespace internal

/**
 *  @brief Creates a transaction from a state object.
 *
 *  Adapts a state type to the Transaction interface. The state type must
 *  provide `apply(Doc&) const` and `undo(Doc&) const` operations, where `Doc` is
 *  inferred from the parameter type of `State::apply`.
 *
 *  The state object is moved into the resulting transaction and must retain
 *  sufficient information to apply and undo the operation.
 *
 *  If State provides a compatible `merge_with(State const&)* operation, the
 *  resulting transaction supports merging with subsequent transactions of
 *  the same concrete state type. The merge operation must satisfy the
 *  contract required by `Transaction::merge_with()`.
 *
 *  The state operations must not throw, as the resulting transaction
 *  exposes `apply()`, `undo()`, and `merge_with()` as `noexcept` operations.
 *
 *  @tparam State The concrete state type describing the transaction.
 *  @tparam Doc The document type, inferred from `State::apply` by default.
 *
 *  @param state The state object to move into the transaction.
 *
 *  @return An owning pointer to the resulting transaction.
 */
template <typename State, typename Doc = internal::StateTransactionTraits<State>::DocumentType>
std::unique_ptr<Transaction<Doc>> make_transaction(State state) {
    return std::make_unique<internal::StateTransactionImpl<Doc, State>>(std::move(state));
}

template <typename Doc>
class DocumentContext {
public:
    explicit DocumentContext(std::shared_ptr<Doc> document):
    m_history(document),
    m_doc(document) {}
    
    void record_transaction(std::unique_ptr<Transaction<Doc>> transaction) {
        transaction->apply(*m_doc);
        if (!m_recording) {
            m_recording = std::move(transaction);
            return;
        }
        if (m_recording->merge_with(*transaction)) {
            return;
        }
        // Here we failed to merge. We commit the current recording and start a new one
        finish_recording();
        m_recording = std::move(transaction);
    }
    
    void finish_recording() {
        if (!m_recording) return;
        m_history.push(std::move(m_recording));
    }
    
    void cancel_recording() {
        if (!m_recording)
            return;
        m_recording->undo(*m_doc);
        m_recording.reset();
    }
    
    void commit_transaction(std::unique_ptr<Transaction<Doc>> transaction) {
        assert(!m_recording);
        transaction->apply(*m_doc);
        m_history.push(std::move(transaction));
    }
    
    Doc const* operator->() const { return m_doc.get(); }
    
private:
    HistoryManager<Doc> m_history;
    std::shared_ptr<Doc> m_doc;
    std::unique_ptr<Transaction<Doc>> m_recording;
};

class MyDocument {
public:
    float get_value() const { return m_value; }
    
    std::unique_ptr<Transaction<MyDocument>> make_value_edit(float new_value) const {
        struct ValueState {
            void apply(MyDocument& self) const {
                self.m_value = new_value;
            }
            void undo(MyDocument& self) const {
                self.m_value = old_value;
            }
            bool merge_with(ValueState const& next) {
                assert(next.old_value == this->new_value);
                new_value = next.new_value;
                return true;
            }
            float old_value, new_value;
        };
        return make_transaction(ValueState{ .old_value = m_value, .new_value = new_value });
    }
    
private:
    float m_value;
};

namespace ImGui {
bool Slider(float*);
bool IsItemDeactivatedAfterEdit();
bool IsKeyPressed(int);
}

int ImGuiKey_Escape;

void display_interface(DocumentContext<MyDocument>& doc_context) {
    // Here we only get a const reference to the document
    float value = doc_context->get_value();
    float old_value = value;
    if (ImGui::Slider(&value)) {
        doc_context.record_transaction(doc_context->make_value_edit(value));
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        doc_context.cancel_recording();
    else if (ImGui::IsItemDeactivatedAfterEdit())
        doc_context.finish_recording();
}
