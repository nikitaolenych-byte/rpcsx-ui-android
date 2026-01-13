/**
 * ╔══════════════════════════════════════════════════════════════════════════╗
 * ║                    NCE v8 - Branch Predictor                             ║
 * ║                                                                          ║
 * ║  Advanced branch prediction with:                                        ║
 * ║  - Two-level adaptive predictor (TAGE-like)                             ║
 * ║  - Loop predictor for counted loops                                     ║
 * ║  - Return address stack (RAS)                                           ║
 * ║  - Indirect branch target buffer (IBTB)                                 ║
 * ╚══════════════════════════════════════════════════════════════════════════╝
 */

#ifndef RPCSX_NCE_V8_BRANCH_PREDICTOR_H
#define RPCSX_NCE_V8_BRANCH_PREDICTOR_H

#include <cstdint>
#include <array>
#include <atomic>

namespace rpcsx::nce::v8 {

// ============================================================================
// Two-Bit Saturating Counter
// ============================================================================
class SaturatingCounter {
public:
    enum State : uint8_t {
        STRONGLY_NOT_TAKEN = 0,
        WEAKLY_NOT_TAKEN = 1,
        WEAKLY_TAKEN = 2,
        STRONGLY_TAKEN = 3
    };
    
    SaturatingCounter() : state_(WEAKLY_NOT_TAKEN) {}
    
    bool Predict() const { return state_ >= WEAKLY_TAKEN; }
    
    void Update(bool taken) {
        if (taken) {
            if (state_ < STRONGLY_TAKEN) state_ = static_cast<State>(state_ + 1);
        } else {
            if (state_ > STRONGLY_NOT_TAKEN) state_ = static_cast<State>(state_ - 1);
        }
    }
    
    uint8_t GetConfidence() const {
        return (state_ == STRONGLY_TAKEN || state_ == STRONGLY_NOT_TAKEN) ? 3 : 1;
    }
    
private:
    State state_;
};

// ============================================================================
// Branch History Table (BHT) - Pattern History Table
// ============================================================================
template<size_t SIZE = 4096>
class BranchHistoryTable {
public:
    BranchHistoryTable() {
        for (auto& entry : table_) {
            entry = SaturatingCounter();
        }
    }
    
    bool Predict(uint64_t pc, uint64_t history) const {
        size_t idx = Hash(pc, history);
        return table_[idx].Predict();
    }
    
    void Update(uint64_t pc, uint64_t history, bool taken) {
        size_t idx = Hash(pc, history);
        table_[idx].Update(taken);
    }
    
private:
    size_t Hash(uint64_t pc, uint64_t history) const {
        return (pc ^ (history << 3)) & (SIZE - 1);
    }
    
    std::array<SaturatingCounter, SIZE> table_;
};

// ============================================================================
// Global History Register
// ============================================================================
class GlobalHistoryRegister {
public:
    GlobalHistoryRegister() : history_(0) {}
    
    uint64_t Get() const { return history_; }
    
    void Update(bool taken) {
        history_ = ((history_ << 1) | (taken ? 1 : 0)) & MASK;
    }
    
    void Reset() { history_ = 0; }
    
private:
    static constexpr uint64_t MASK = (1ULL << 64) - 1;
    uint64_t history_;
};

// ============================================================================
// Loop Predictor
// ============================================================================
class LoopPredictor {
public:
    static constexpr size_t TABLE_SIZE = 256;
    
    struct LoopEntry {
        uint64_t tag;                // PC tag
        uint16_t trip_count;         // Detected loop trip count
        uint16_t current_iter;       // Current iteration
        uint8_t confidence;          // Confidence in trip count
        bool valid;
    };
    
    LoopPredictor() {
        for (auto& entry : table_) {
            entry.valid = false;
        }
    }
    
    // Returns: true if prediction made, result in 'prediction'
    bool Predict(uint64_t pc, bool& prediction) {
        size_t idx = pc & (TABLE_SIZE - 1);
        auto& entry = table_[idx];
        
        if (!entry.valid || entry.tag != (pc >> 8)) {
            return false;  // No prediction
        }
        
        if (entry.confidence < 3) {
            return false;  // Low confidence
        }
        
        // Predict taken until trip count reached
        prediction = (entry.current_iter < entry.trip_count);
        return true;
    }
    
    void Update(uint64_t pc, bool taken) {
        size_t idx = pc & (TABLE_SIZE - 1);
        auto& entry = table_[idx];
        uint64_t tag = pc >> 8;
        
        if (!entry.valid || entry.tag != tag) {
            // New loop
            entry.tag = tag;
            entry.trip_count = 0;
            entry.current_iter = 0;
            entry.confidence = 0;
            entry.valid = true;
        }
        
        if (taken) {
            entry.current_iter++;
        } else {
            // Loop exit - update trip count
            if (entry.current_iter == entry.trip_count) {
                entry.confidence = std::min(7, entry.confidence + 1);
            } else {
                entry.trip_count = entry.current_iter;
                entry.confidence = 1;
            }
            entry.current_iter = 0;
        }
    }
    
private:
    std::array<LoopEntry, TABLE_SIZE> table_;
};

// ============================================================================
// Return Address Stack (RAS)
// ============================================================================
class ReturnAddressStack {
public:
    static constexpr size_t STACK_SIZE = 32;
    
    ReturnAddressStack() : top_(0) {
        stack_.fill(0);
    }
    
    void Push(uint64_t return_addr) {
        stack_[top_ & (STACK_SIZE - 1)] = return_addr;
        top_++;
    }
    
    uint64_t Pop() {
        if (top_ == 0) return 0;
        top_--;
        return stack_[top_ & (STACK_SIZE - 1)];
    }
    
    uint64_t Peek() const {
        if (top_ == 0) return 0;
        return stack_[(top_ - 1) & (STACK_SIZE - 1)];
    }
    
    bool IsEmpty() const { return top_ == 0; }
    
private:
    std::array<uint64_t, STACK_SIZE> stack_;
    uint32_t top_;
};

// ============================================================================
// Indirect Branch Target Buffer (IBTB)
// ============================================================================
class IndirectBranchTargetBuffer {
public:
    static constexpr size_t TABLE_SIZE = 512;
    
    struct Entry {
        uint64_t pc_tag;
        uint64_t target;
        uint8_t confidence;
        bool valid;
    };
    
    IndirectBranchTargetBuffer() {
        for (auto& entry : table_) {
            entry.valid = false;
        }
    }
    
    bool Predict(uint64_t pc, uint64_t& target) const {
        size_t idx = pc & (TABLE_SIZE - 1);
        const auto& entry = table_[idx];
        
        if (entry.valid && entry.pc_tag == (pc >> 9) && entry.confidence >= 2) {
            target = entry.target;
            return true;
        }
        return false;
    }
    
    void Update(uint64_t pc, uint64_t actual_target) {
        size_t idx = pc & (TABLE_SIZE - 1);
        auto& entry = table_[idx];
        uint64_t tag = pc >> 9;
        
        if (!entry.valid || entry.pc_tag != tag) {
            // New entry
            entry.pc_tag = tag;
            entry.target = actual_target;
            entry.confidence = 1;
            entry.valid = true;
        } else if (entry.target == actual_target) {
            // Hit - increase confidence
            entry.confidence = std::min(7, entry.confidence + 1);
        } else {
            // Misprediction
            if (entry.confidence <= 1) {
                entry.target = actual_target;
                entry.confidence = 1;
            } else {
                entry.confidence--;
            }
        }
    }
    
private:
    std::array<Entry, TABLE_SIZE> table_;
};

// ============================================================================
// Combined Branch Predictor (TAGE-like)
// ============================================================================
class CombinedBranchPredictor {
public:
    CombinedBranchPredictor();
    ~CombinedBranchPredictor() = default;
    
    struct Prediction {
        bool taken;
        uint64_t target;           // For indirect branches
        uint8_t confidence;        // 0-7
        enum Source {
            BIMODAL,
            LOCAL,
            GLOBAL,
            LOOP,
            INDIRECT,
            RAS
        } source;
    };
    
    // Main prediction interface
    Prediction Predict(uint64_t pc, bool is_call, bool is_return, bool is_indirect);
    
    // Update after actual branch resolution
    void Update(uint64_t pc, bool taken, uint64_t actual_target,
                bool is_call, bool is_return, bool is_indirect);
    
    // Stats
    uint64_t GetPredictions() const { return total_predictions_; }
    uint64_t GetCorrect() const { return correct_predictions_; }
    uint64_t GetMispredictions() const { return total_predictions_ - correct_predictions_; }
    double GetAccuracy() const {
        return total_predictions_ > 0 
            ? (double)correct_predictions_ / total_predictions_ * 100.0 
            : 0.0;
    }
    
    void ResetStats() {
        total_predictions_ = 0;
        correct_predictions_ = 0;
    }
    
private:
    // Bimodal predictor (fallback)
    BranchHistoryTable<4096> bimodal_;
    
    // Local history predictor
    std::array<uint16_t, 1024> local_history_;
    BranchHistoryTable<4096> local_bht_;
    
    // Global history predictor
    GlobalHistoryRegister global_history_;
    BranchHistoryTable<8192> global_bht_;
    
    // Specialized predictors
    LoopPredictor loop_predictor_;
    ReturnAddressStack ras_;
    IndirectBranchTargetBuffer ibtb_;
    
    // Tournament selector: chooses between local and global
    BranchHistoryTable<4096> selector_;
    
    // Stats
    std::atomic<uint64_t> total_predictions_{0};
    std::atomic<uint64_t> correct_predictions_{0};
};

// ============================================================================
// Implementation
// ============================================================================

inline CombinedBranchPredictor::CombinedBranchPredictor() {
    local_history_.fill(0);
}

inline CombinedBranchPredictor::Prediction 
CombinedBranchPredictor::Predict(uint64_t pc, bool is_call, bool is_return, bool is_indirect) {
    total_predictions_++;
    Prediction pred;
    pred.confidence = 3;
    pred.target = pc + 4;  // Default: fall-through
    
    // Handle returns with RAS
    if (is_return) {
        pred.target = ras_.Peek();
        pred.taken = true;
        pred.confidence = ras_.IsEmpty() ? 1 : 7;
        pred.source = Prediction::RAS;
        return pred;
    }
    
    // Handle indirect branches
    if (is_indirect) {
        if (ibtb_.Predict(pc, pred.target)) {
            pred.taken = true;
            pred.confidence = 5;
            pred.source = Prediction::INDIRECT;
            return pred;
        }
    }
    
    // Try loop predictor first
    bool loop_pred;
    if (loop_predictor_.Predict(pc, loop_pred)) {
        pred.taken = loop_pred;
        pred.confidence = 6;
        pred.source = Prediction::LOOP;
        return pred;
    }
    
    // Tournament between local and global
    size_t local_idx = pc & 1023;
    uint16_t local_hist = local_history_[local_idx];
    
    bool local_pred = local_bht_.Predict(pc, local_hist);
    bool global_pred = global_bht_.Predict(pc, global_history_.Get());
    
    // Selector chooses which to use
    bool use_global = selector_.Predict(pc, global_history_.Get());
    
    if (use_global) {
        pred.taken = global_pred;
        pred.source = Prediction::GLOBAL;
    } else {
        pred.taken = local_pred;
        pred.source = Prediction::LOCAL;
    }
    
    pred.confidence = 4;
    return pred;
}

inline void CombinedBranchPredictor::Update(uint64_t pc, bool taken, uint64_t actual_target,
                                             bool is_call, bool is_return, bool is_indirect) {
    // Update RAS for calls/returns
    if (is_call) {
        ras_.Push(pc + 4);
    } else if (is_return) {
        ras_.Pop();
    }
    
    // Update indirect branch predictor
    if (is_indirect) {
        ibtb_.Update(pc, actual_target);
    }
    
    // Update loop predictor
    loop_predictor_.Update(pc, taken);
    
    // Get predictions for selector update
    size_t local_idx = pc & 1023;
    uint16_t local_hist = local_history_[local_idx];
    
    bool local_pred = local_bht_.Predict(pc, local_hist);
    bool global_pred = global_bht_.Predict(pc, global_history_.Get());
    
    // Update selector if predictions differ
    if (local_pred != global_pred) {
        bool global_correct = (global_pred == taken);
        selector_.Update(pc, global_history_.Get(), global_correct);
    }
    
    // Update local predictor
    local_bht_.Update(pc, local_hist, taken);
    local_history_[local_idx] = ((local_hist << 1) | (taken ? 1 : 0)) & 0x3FF;
    
    // Update global predictor
    global_bht_.Update(pc, global_history_.Get(), taken);
    global_history_.Update(taken);
    
    // Update bimodal (always)
    bimodal_.Update(pc, 0, taken);
    
    // Track accuracy
    // (Note: would need to store last prediction to verify)
}

} // namespace rpcsx::nce::v8

#endif // RPCSX_NCE_V8_BRANCH_PREDICTOR_H
