// // FOR THE CUSTOM PREDICTOR
// // Enhanced TAGE-SC-L Predictor
// #define BIMODAL_SIZE 8192       // 8K entries
// #define TAGE_MAX_SIZE (1 << 12)  // Increased table size

// // Statistical Corrector parameters
// #define SC_TABLES 4
// #define SC_COUNTER_BITS 3
// #define SC_HIST_LEN 256

// // Loop Predictor parameters
// #define LOOP_ENTRIES 256
// #define LOOP_TAG_BITS 14
// #define MAX_ITERATIONS 16383  // 14-bit counter

// // TAGE configuration
// #define NUM_TABLES 12
// const uint32_t HIST_LENGTHS[NUM_TABLES] = {4, 6, 10, 16, 25, 40, 64, 101, 160, 254, 403, 640};
// const uint32_t TAG_WIDTH[NUM_TABLES] = {9, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19};
// const uint32_t TABLE_SIZE[NUM_TABLES] = {11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11};

// typedef struct {
//     uint16_t tag;
//     uint8_t ctr;     // 3-bit counter
//     uint8_t useful;  // 2-bit usefulness counter
// } TageEntry;

// typedef struct {
//     uint16_t tag;
//     uint16_t period;
//     uint16_t current;
//     uint8_t confidence;
//     uint8_t age;
// } LoopEntry;

// // Prediction components
// uint8_t bimodal[BIMODAL_SIZE];
// TageEntry tage_tables[NUM_TABLES][TAGE_MAX_SIZE];
// LoopEntry loop_predictor[LOOP_ENTRIES];
// int8_t sc_tables[SC_TABLES][SC_HIST_LEN];
// uint32_t path_hist;   // Path History
// uint16_t lhist[512];  // Local History

// #define GHR_BITS 640
// #define GHR_WORDS ((GHR_BITS + 63) / 64)  // 10 x 64-bit words = 640 bits

// uint64_t ghr[GHR_WORDS];  // 640-bit global history


// // Optimized folding function from BADGR paper [3][9]
// uint32_t fold_ghr(uint32_t history_length, uint32_t output_bits) {
//     uint32_t folded = 0;
//     uint32_t chunks = (history_length + 63) / 64;
    
//     for(int i = 0; i < chunks; i++) {
//         folded ^= (ghr[i] & ((1ULL << (history_length % 64)) - 1));
//         history_length = history_length > 64 ? history_length - 64 : 0;
//     }
    
//     // Final compression
//     folded ^= folded >> (output_bits / 2);
//     folded ^= folded >> (output_bits / 4);
//     return folded & ((1 << output_bits) - 1);
// }

// // Updated GHR shift from GL-TAGE paper [1]
// void update_ghr(uint8_t outcome) {
//     // Shift entire GHR array with carry-over
//     uint64_t carry = outcome;
//     for(int i = 0; i < GHR_WORDS; i++) {
//         uint64_t new_carry = ghr[i] >> 63;
//         ghr[i] = (ghr[i] << 1) | carry;
//         carry = new_carry;
//     }
// }

// // Modified index calculation from L-TAGE [4][12]
// uint32_t tage_index(uint32_t table, uint32_t pc) {
//     uint32_t hist_len = HIST_LENGTHS[table];
//     uint32_t folded_hist = fold_ghr(hist_len, TABLE_SIZE[table]);
//     return (pc ^ folded_hist) & ((1 << TABLE_SIZE[table]) - 1);
// }

// // Updated tag calculation using path history [4][24]
// uint16_t tage_tag(uint32_t table, uint32_t pc, uint32_t path_hist) {
//     uint32_t hist_len = HIST_LENGTHS[table];
//     uint32_t folded_hist = fold_ghr(hist_len, TAG_WIDTH[table]);
//     return (pc ^ folded_hist ^ path_hist) & ((1 << TAG_WIDTH[table]) - 1);
// }

// uint16_t loop_tag(uint64_t pc) {
//     return (pc ^ (pc >> 16) ^ (pc >> 32)) & ((1 << LOOP_TAG_BITS) - 1);
// }

// // Initialization
// void init_custom() {
//     // Initialize bimodal
//     for (int i = 0; i < BIMODAL_SIZE; i++)
//     {
//         bimodal[i] = WT;
//     }
    
//     // Initialize TAGE tables
//     for(int t=0; t<NUM_TABLES; t++) {
//         for(int i=0; i<TAGE_MAX_SIZE; i++) {
//             tage_tables[t][i].tag = 0;
//             tage_tables[t][i].ctr = 4;
//             tage_tables[t][i].useful = 0;
//         }
//     }
    
//     // Initialize loop predictor
//     for (int i = 0; i < LOOP_ENTRIES; i++) {
//         loop_predictor[i].tag = 0;
//         loop_predictor[i].period = 0;
//         loop_predictor[i].current = 0;
//         loop_predictor[i].confidence = 0;
//         loop_predictor[i].age = 0;
//     }
    
//     // Initialize statistical corrector
//     for (int i = 0; i < SC_TABLES; i++) {
//         for (int j = 0; j < SC_HIST_LEN; j++) {
//             sc_tables[i][j] = 0;
//         }
//     }
    
//     // Initialize histories
//     for (int i = 0; i < 10; i++)
//         ghr[i] = 0;

//     path_hist = 0;
    
//     for (int i = 0; i < 512; i++) {
//         lhist[i] = 0;
//     }
// }

// uint8_t custom_predict(uint64_t pc) {
//     // Loop prediction first
//     uint16_t ltag = loop_tag(pc);
//     uint16_t lindex = pc % LOOP_ENTRIES;
//     LoopEntry *lentry = &loop_predictor[lindex];
    
//     if(lentry->tag == ltag && lentry->confidence > 1) {
//         if(++lentry->current >= lentry->period) {
//             return NOTTAKEN;  // Predict loop exit
//         }
//         return TAKEN;  // Predict loop continuation
//     }

//     // TAGE prediction
//     uint8_t tage_pred = bimodal[pc % BIMODAL_SIZE] >= 4;
//     int provider = -1;
    
//     for(int t=NUM_TABLES-1; t>=0; t--) {
//         uint32_t idx = tage_index(t, pc);
//         uint16_t tag = tage_tag(t, pc, path_hist);
        
//         if(tage_tables[t][idx].tag == tag) {
//             provider = t;
//             tage_pred = tage_tables[t][idx].ctr >= 4;
//             break;
//         }
//     }

//     // Statistical corrector
//     int sc_output = 0;
//     uint32_t sc_idx = (pc ^ path_hist) & (SC_HIST_LEN-1);
//     for(int i=0; i<SC_TABLES; i++) {
//         sc_output += sc_tables[i][(sc_idx + i) % SC_HIST_LEN];
//     }
    
//     // Combine predictions
//     return (sc_output > 2 * SC_COUNTER_BITS) ? !tage_pred : tage_pred;
// }

// void train_custom(uint64_t pc, uint8_t outcome) {
//     // Update loop predictor
//     uint16_t lindex = pc % LOOP_ENTRIES;
//     LoopEntry *lentry = &loop_predictor[lindex];
    
//     if(lentry->tag == loop_tag(pc)) {
//         if(outcome == TAKEN) {
//             if(++lentry->current == lentry->period) {
//                 lentry->confidence = INC_CNTR(lentry->confidence);
//                 lentry->current = 0;
//             }
//         } else {
//             lentry->confidence = DEC_CNTR(lentry->confidence);
//         }
//     } else {
//         if(lentry->confidence == 0) {
//             lentry->tag = loop_tag(pc);
//             lentry->period = lentry->current;
//             lentry->current = 0;
//             lentry->confidence = 1;
//         }
//     }

//     // Update TAGE
//     int provider = -1;
//     bool tage_correct = true;
//     int altpred = -1;
//     bool alloc = false;    
    
//     // TAGE prediction
//     uint8_t tage_pred = bimodal[pc % BIMODAL_SIZE] >= 4;

//     // Phase 1: Find provider and alternative prediction
//     for(int t = NUM_TABLES-1; t >= 0; t--) {
//         uint32_t idx = tage_index(t, pc);
//         uint16_t tag = tage_tag(t, pc, path_hist);
        
//         if(tage_tables[t][idx].tag == tag) {
//             if(provider == -1) {
//                 provider = t;
//                 tage_pred = tage_tables[t][idx].ctr >= 4;
//                 altpred = (t > 0) ? 
//                     (tage_tables[t-1][tage_index(t-1, pc)].ctr >= 4) : 
//                     (bimodal[pc % BIMODAL_SIZE] >= 4);
//             }
//             tage_tables[t][idx].useful = DEC_CNTR(tage_tables[t][idx].useful);
//         }
//     }

//     // Phase 2: Update provider component [15]
//     if(provider != -1) {
//         TageEntry *entry = &tage_tables[provider][tage_index(provider, pc)];
        
//         // Update counter
//         entry->ctr = outcome ? INC_3B_CNTR(entry->ctr) : 
//                               DEC_3B_CNTR(entry->ctr);
        
//         // Update usefulness [9]
//         if(outcome == tage_pred && tage_pred != altpred) {
//             entry->useful = INC_CNTR(entry->useful);
//         }
//     }

//     // Phase 3: Allocation logic [1][15]
//     if((provider == -1) || (tage_pred != outcome)) {
//         int start = (provider == -1) ? 0 : (provider + 1);
//         start = MIN(start, NUM_TABLES-1);
        
//         // Find allocation candidate [9]
//         for(int t = start; t < NUM_TABLES; t++) {
//             uint32_t idx = tage_index(t, pc);
//             TageEntry *entry = &tage_tables[t][idx];

//             if(entry->useful == 0) {
//                 entry->tag = tage_tag(t, pc, path_hist);
//                 entry->ctr = outcome ? 6 : 2;  // Strong initial bias [15]
//                 entry->useful = 0;  // Reset usefulness
//                 alloc = true;
//                 break;
//             }
//         }

//         // Reset usefulness periodically [1]
//         if(!alloc && (rand() % 256 == 0)) { 
//             for(int t = start; t < NUM_TABLES; t++) {
//                 uint32_t idx = tage_index(t, pc);
//                 tage_tables[t][idx].useful >>= 1;  // Age usefulness
//             }
//         }
//     }

//     // Update statistical corrector
//     uint32_t sc_idx = (pc ^ path_hist) & (SC_HIST_LEN-1);
//     for(int i=0; i<SC_TABLES; i++) {
//         if((tage_correct && sc_tables[i][sc_idx] > 0) ||
//            (!tage_correct && sc_tables[i][sc_idx] < 0)) {
//             sc_tables[i][sc_idx] += (outcome ? 1 : -1);
//             sc_tables[i][sc_idx] = CLAMP(sc_tables[i][sc_idx], 
//                                       -(1 << (SC_COUNTER_BITS-1)), 
//                                       (1 << (SC_COUNTER_BITS-1))-1);
//         }
//     }

//     // Update histories
//     update_ghr(outcome);
//     path_hist = (path_hist << 1) | (pc & 1);
//     lhist[pc % 512] = (lhist[pc % 512] << 1) | outcome;
// }


// // FOR THE CUSTOM PREDICTOR
//TAGE Predictor
// #define BIMODAL_SIZE 8192       // 8K entries
// #define TAGE_MAX_SIZE (1 << 11)

// #define c_p_idx_bits 10

// #define c_p_idx_size (1 << c_p_idx_bits)
// #define c_p_pc_size 16

// int8_t W[c_p_idx_size][c_p_pc_size];

// #define NUM_TABLES 12

// const uint32_t HIST_LENGTHS[NUM_TABLES] = {4, 6, 10, 16, 25, 40, 64, 101, 160, 254, 403, 640};
// const uint32_t TAG_WIDTH[NUM_TABLES] = {7, 7, 8, 8, 9, 10, 11, 12, 12, 13, 14, 15};
// const uint32_t TABLE_SIZE[NUM_TABLES] =
//     {
//         10,
//         10,
//         11,
//         11,
//         11,
//         11,
//         11,
//         10,
//         10,
//         10,
//         10,
//         9
// };

// typedef struct
// {
//     uint16_t tag;
//     uint8_t ctr;
//     uint8_t useful;
// } TageEntry;

// uint8_t bimodal[BIMODAL_SIZE];
// TageEntry tage_tables[NUM_TABLES][TAGE_MAX_SIZE];

// uint64_t ghr[10]; // to store 640 bits

// uint64_t fold_hash(uint64_t *val, uint32_t bits, uint32_t sz)
// {

//     uint64_t folded = 0;
//     uint32_t i = 0;

//     return (val[0] & ((1 << sz) - 1));

//     for (i = 0; i < (bits - 1) / 64; i++)
//     {
//         folded ^= val[i];
//     }

//     if (i < 10)
//     {
//         if (bits % 64 == 0)
//         {
//             folded ^= val[i];
//         }
//         else
//         {
//             uint64_t nbits = bits % 64;
//             nbits = ((uint64_t)1 << nbits) - 1;
//             folded ^= (val[i] & nbits);
//         }
//     }

//     uint64_t final_val = 0;
//     for (i = 0; i < 63 / sz; i++)
//     {
//         uint64_t tmp = (folded >> (i * sz)) & (((uint64_t)1 << sz) - 1);
//         final_val ^= tmp;
//     }

//     uint64_t tmp = (folded >> (i * sz)) & (((uint64_t)1 << (64 % sz)) - 1);
//     final_val ^= tmp;

//     final_val = final_val % (1 << sz);

//     return final_val;
// }

// void init_custom()
// {
//     // Initialize bimodal table
//     for (int i = 0; i < BIMODAL_SIZE; i++)
//     {
//         bimodal[i] = WT;
//     }

//     // Initialize TAGE tables
//     for (int t = 0; t < NUM_TABLES; t++)
//     {
//         for (int i = 0; i < TAGE_MAX_SIZE; i++)
//         {
//             tage_tables[t][i].tag = 0;
//             tage_tables[t][i].ctr = 4; // Weakly taken
//             tage_tables[t][i].useful = 0;
//         }
//     }

//     for(int i = 0; i < c_p_idx_size; i++) {
//         for(int j = 0; j < c_p_pc_size; j++) {
//             W[i][j] = 0;
//         }
//     }

//     // Initialize history
//     for (int i = 0; i < 10; i++)
//         ghr[i] = 0;
// }

// static int x = 0;
// uint8_t custom_predict(uint32_t pc)
// {
//     // Base prediction from bimodal

//     uint64_t bimodal_idx = pc & ( (1 << c_p_idx_bits ) - 1);
//     int64_t y = 0;

//     for (int i = 0; i < c_p_pc_size; i ++) {
//         y = y + W[bimodal_idx][c_p_pc_size - 1 - i] * ((ghr[i/64] >> i) & 1);
//     }

//     uint8_t pred = (y > 0) ? TAKEN : NOTTAKEN ;

//     int provider = -1;

//     // TAGE prediction
//     for (int t = NUM_TABLES - 1; t >= 0; t--)
//     {
//         uint32_t idx = ((pc % (1 << TABLE_SIZE[t])) ^ fold_hash(ghr, HIST_LENGTHS[t], TABLE_SIZE[t]));
//         uint16_t tag = ((pc >> 2) ^ (ghr[0] << 4)) & (((uint64_t)1 << TAG_WIDTH[t]) - 1);

//         if (tage_tables[t][idx].tag & (((uint64_t)1 << TAG_WIDTH[t]) - 1) == tag)
//         {
//             if (tage_tables[t][idx].useful != 0) {
//                 provider = t;
//                 pred = tage_tables[t][idx].ctr >= 4;
//                 break;
//             }
//         }
//     }

//     return pred;
// }

// void train_custom(uint32_t pc, uint8_t outcome)
// {
//     // Update bimodal
//     uint64_t bimodal_idx = pc & ( (1 << c_p_idx_bits ) - 1 );
//     int64_t y = 0;

//     for (int i = 0; i < c_p_pc_size; i ++) {
//         y = y + W[bimodal_idx][c_p_pc_size - 1 - i] * ((ghr[i/64] >> i) & 1);
//     }

//     int corr = (outcome == TAKEN) ? 1 : -1;
//     if( ((y < 0) != (corr < 0)) || (y < c_p_pc_size/2 && y > -c_p_pc_size/2) )
//     {
//         for(int i = 0; i < c_p_pc_size; i++)
//         {
//             W[bimodal_idx][c_p_pc_size - 1 - i] += ((ghr[i/64] >> i) & 1) * corr;
//         }           
//     }

//     // TAGE update
//     int provider = -1;
//     int alloc_table = -1;

//     uint8_t tage_pred = custom_predict(pc);

//     // Find provider and potential allocation candidates
//     for (int t = NUM_TABLES - 1; t >= 0; t--)
//     {
//         uint32_t idx = ((pc % (1 << TABLE_SIZE[t])) ^ fold_hash(ghr, HIST_LENGTHS[t], TABLE_SIZE[t]));
//         uint16_t tag = ((pc >> 2) ^ (ghr[0] << 4)) & (((uint64_t)1 << TAG_WIDTH[t]) - 1);

//         if (tage_tables[t][idx].tag == (tag & (((uint64_t)1 << TAG_WIDTH[t]) - 1)))
//         {
//             if (provider == -1)
//             {
//                 provider = t;
//                 // Update counter
//                 if (outcome)
//                 {
//                     tage_tables[t][idx].ctr = INC_3B_CNTR(tage_tables[t][idx].ctr);
//                 }
//                 else
//                 {
//                     tage_tables[t][idx].ctr = DEC_3B_CNTR(tage_tables[t][idx].ctr);
//                 }
//                 if (outcome == tage_pred)
//                 {
//                     tage_tables[t][idx].useful = INC_CNTR(tage_tables[t][idx].useful);
//                     tage_tables[t][idx].useful = INC_CNTR(tage_tables[t][idx].useful);
//                 }
//             }
//             // Update usefulness
//             tage_tables[t][idx].useful = DEC_CNTR(tage_tables[t][idx].useful);
//         }
//         else if (t < alloc_table && tage_tables[t][idx].useful == 0)
//         {
//             alloc_table = t;
//         }
//     }

//     uint32_t p_idx = provider != -1 ? ((pc % (1 << TABLE_SIZE[provider])) ^ fold_hash(ghr, HIST_LENGTHS[provider], TABLE_SIZE[provider])) : -1;

//     // Allocation logic
//     if ((provider == -1) || (tage_tables[provider][p_idx].ctr >= 4) != outcome)
//     {
//         // if (bimodal_update) {
//         if (alloc_table == -1)
//         {
//             // Find table with least useful entries
//             uint8_t min_useful = 7;
//             for (int t = 0; t < NUM_TABLES; t++)
//             {
//                 uint32_t idx = ((pc % (1 << TABLE_SIZE[t])) ^ fold_hash(ghr, HIST_LENGTHS[t], TABLE_SIZE[t]));
//                 if (tage_tables[t][idx].useful < min_useful)
//                 {
//                     min_useful = tage_tables[t][idx].useful;
//                     alloc_table = t;
//                 }
//             }
//         }

//         if (alloc_table != -1)
//         {
//             uint32_t idx = ((pc % (1 << TABLE_SIZE[alloc_table])) ^ fold_hash(ghr, HIST_LENGTHS[alloc_table], TABLE_SIZE[alloc_table]));
//             tage_tables[alloc_table][idx].tag = ((pc >> 2) ^ (ghr[0] << 4)) & (((uint64_t)1 << TAG_WIDTH[alloc_table]) - 1);
//             tage_tables[alloc_table][idx].ctr = outcome ? 6 : 2;
//             tage_tables[alloc_table][idx].useful = 3;
//         }
//         // }
//     }
//     // Update history
//     for (int i = 10; i > 0; i--)
//     {
//         uint8_t x = (ghr[i - 1] >> 63) & 1;
//         ghr[i] = (ghr[i] << 1) | x;
//     }
//     ghr[0] = (ghr[0] << 1) | outcome;
// }


// FOR THE CUSTOM PREDICTOR

// // Perceptron predictor

// #define c_p_idx_bits 10

// #define c_p_idx_size (1 << c_p_idx_bits)
// #define c_p_pc_size 16

// int8_t W[c_p_idx_size][c_p_pc_size];

// void init_custom()
// {
    
//     for (int i = 0; i < 10; i++)
//         c_ghr[i] = 0;

//     for(int i = 0; i < c_p_idx_size; i++) {
//         for(int j = 0; j < c_p_pc_size; j++) {
//             W[i][j] = 0;
//         }
//     }
// }

// uint8_t custom_predict(uint32_t pc)
// {
//     uint64_t index = pc & ( (1 << c_p_idx_bits ) - 1);
//     int64_t y = 0;

//     for (int i = 0; i < c_p_pc_size; i ++) {
//         y = y + W[index][c_p_pc_size - 1 - i] * ((c_ghr[i/64] >> i) & 1);
//     }

//     return (y > 0) ? TAKEN : NOTTAKEN ;
// }

// void train_custom(uint32_t pc, uint8_t outcome)
// {
//     uint64_t index = pc & ( (1 << c_p_idx_bits ) - 1 );
//     int64_t y = 0;

//     for (int i = 0; i < c_p_pc_size; i ++) {
//         y = y + W[index][c_p_pc_size - 1 - i] * ((c_ghr[i/64] >> i) & 1);
//     }

//     int corr = (outcome == TAKEN) ? 1 : -1;
//     if( ((y < 0) != (corr < 0)) || (y < c_p_pc_size/2 && y > -c_p_pc_size/2) )
//     {
//         for(int i = 0; i < c_p_pc_size; i++)
//         {
//             W[index][c_p_pc_size - 1 - i] += ((c_ghr[i/64] >> i) & 1) * corr;
//         }           
//     }
//     for (int i = 10; i > 0; i--)
//     {
//         uint8_t x = (c_ghr[i - 1] >> 63) & 1;
//         c_ghr[i] = (c_ghr[i] << 1) | x;
//     }
//     c_ghr[0] = (c_ghr[0] << 1) | outcome;
// }
