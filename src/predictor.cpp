//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include <math.h>
#include "predictor.h"

//
// Student Information
//
const char *studentName = "Mayank Kumar";
const char *studentID = "A69030454";
const char *email = "mak025@ucsd.edu";

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = {"Static", "Gshare",
                         "Tournament", "Custom"};

// define number of bits required for indexing the BHT here.
int ghistoryBits = 17; // Number of bits used for Global History
int bpType;            // Branch Prediction Type
int verbose;

//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

//
// TODO: Add your own Branch Predictor data structures here
//
// gshare
uint8_t *bht_gshare;
uint64_t ghistory;

//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
//

// gshare functions
void init_gshare()
{
    int bht_entries = 1 << ghistoryBits;
    bht_gshare = (uint8_t *)malloc(bht_entries * sizeof(uint8_t));
    int i = 0;
    for (i = 0; i < bht_entries; i++)
    {
        bht_gshare[i] = WN;
    }
    ghistory = 0;
}

uint8_t gshare_predict(uint32_t pc)
{
    // get lower ghistoryBits of pc
    uint32_t bht_entries = 1 << ghistoryBits;
    uint32_t pc_lower_bits = pc & (bht_entries - 1);
    uint32_t ghistory_lower_bits = ghistory & (bht_entries - 1);
    uint32_t index = pc_lower_bits ^ ghistory_lower_bits;
    switch (bht_gshare[index])
    {
    case WN:
        return NOTTAKEN;
    case SN:
        return NOTTAKEN;
    case WT:
        return TAKEN;
    case ST:
        return TAKEN;
    default:
        printf("Warning: Undefined state of entry in GSHARE BHT!\n");
        return NOTTAKEN;
    }
}

void train_gshare(uint32_t pc, uint8_t outcome)
{
    // get lower ghistoryBits of pc
    uint32_t bht_entries = 1 << ghistoryBits;
    uint32_t pc_lower_bits = pc & (bht_entries - 1);
    uint32_t ghistory_lower_bits = ghistory & (bht_entries - 1);
    uint32_t index = pc_lower_bits ^ ghistory_lower_bits;

    // Update state of entry in bht based on outcome
    switch (bht_gshare[index])
    {
    case WN:
        bht_gshare[index] = (outcome == TAKEN) ? WT : SN;
        break;
    case SN:
        bht_gshare[index] = (outcome == TAKEN) ? WN : SN;
        break;
    case WT:
        bht_gshare[index] = (outcome == TAKEN) ? ST : WN;
        break;
    case ST:
        bht_gshare[index] = (outcome == TAKEN) ? ST : WT;
        break;
    default:
        printf("Warning: Undefined state of entry in GSHARE BHT!\n");
        break;
    }

    // Update history register
    ghistory = ((ghistory << 1) | outcome);
}

void cleanup_gshare()
{
    free(bht_gshare);
}

// 21264 Alpha processor : tournament predictor

// t = table
// r = register
bool isCustom = false;


#define tourney_lhistoryBits 12 // Number of bits used for Local History
#define tourney_ghistoryBits 16 // Number of bits used for Global History
#define tourney_choiceBits 15   // Number of bits used for Choice Table

uint64_t tourney_global_hr;

uint32_t tourney_local_ht[1 << tourney_lhistoryBits];


uint8_t tourney_local_pred[1 << tourney_lhistoryBits];
uint8_t tourney_global_pred[1 << tourney_ghistoryBits];
uint8_t tourney_choice_pred[1 << tourney_choiceBits];


// Tourney Predictor :
//  tourament functions

#define LTB_ENTRIES 256  // Loop Termination Buffer entries
#define MAX_LOOP_COUNT 1024
#define CONF_THRESH 6    // Confidence threshold for stable prediction

typedef struct {
    uint32_t tag;           // Partial PC signature
    uint16_t max_count;     // Predicted loop trip count
    uint16_t current_count; // Current iteration count
    uint8_t confidence;     // Confidence counter (0-7)
    uint8_t valid;          // Valid entry flag
    uint8_t age;            // For LRU replacement
} LTB_Entry;

LTB_Entry *ltb;

// Initialize Loop Termination Buffer
void init_loop_predictor() {
    ltb = (LTB_Entry *)calloc(LTB_ENTRIES, sizeof(LTB_Entry));
}

// Loop prediction logic
uint8_t loop_predict(uint32_t pc) {

    uint32_t index = pc % LTB_ENTRIES;
    LTB_Entry *entry = &ltb[index];
    
    // Tag match check
    if (entry->valid && entry->tag == (pc & 0xFFFF)) {
        if (entry->confidence >= CONF_THRESH) {
            return (entry->current_count < entry->max_count) ? TAKEN : NOTTAKEN;
        }
    }
    
    // Default prediction for undetected loops
    return 0xFF;
}

// Train loop predictor
void train_loop_predictor(uint32_t pc, uint8_t outcome) {

    uint32_t index = pc % LTB_ENTRIES;
    LTB_Entry *entry = &ltb[index];
    
    // Detect new loops or update existing entries
    if (entry->valid && entry->tag == (pc & 0xFFFF)) {
        if (outcome == TAKEN) {
            entry->current_count++;
        } else {
            // Loop exit - update confidence and max_count
            if (entry->current_count == entry->max_count) {
                entry->confidence = (entry->confidence < 7) ? entry->confidence + 1 : 7;
            } else {
                entry->max_count = entry->current_count;
                entry->confidence = 0;
            }
            entry->current_count = 0;
        }
    } else {
        // Allocate new entry on first loop iteration
        if (outcome == TAKEN) {
            entry->tag = pc & 0xFFFF;
            entry->valid = 1;
            entry->current_count = 1;
            entry->max_count = MAX_LOOP_COUNT;
            entry->confidence = 0;
            entry->age = 0;
        }
    }

}



void init_tourney()
{
    int local_bht_entries = 1 << tourney_lhistoryBits;
    // tourney_local_pred = (uint8_t *)malloc(local_bht_entries * sizeof(uint8_t));
    // tourney_local_ht = (uint32_t *)malloc(local_bht_entries * sizeof(uint32_t));
    int i = 0;
    for (i = 0; i < local_bht_entries; i++)
    {
        tourney_local_pred[i] = WN;
        tourney_local_ht[i] = 0;
    }

    int global_bht_entries = 1 << tourney_ghistoryBits;
    // tourney_global_pred = (uint8_t *)malloc(global_bht_entries * sizeof(uint8_t));
    for (i = 0; i < global_bht_entries; i++)
    {
        tourney_global_pred[i] = WN;
    }

    int choice_t_entries = 1 << tourney_choiceBits;
    // tourney_choice_pred = (uint8_t *)malloc(choice_t_entries * sizeof(uint8_t));
    for (i = 0; i < choice_t_entries; i++)
    {
        tourney_choice_pred[i] = WT;
    }
    tourney_global_hr = 0;

    if (isCustom)
        init_loop_predictor();
}

uint8_t tourney_predict_global(uint32_t pc)
{
    uint32_t global_bht_entries = 1 << tourney_ghistoryBits;
    uint32_t index = 0;
    if (isCustom){
        index = (pc ^ tourney_global_hr) & (global_bht_entries - 1);
    } else {
        index = tourney_global_hr & (global_bht_entries - 1);
    }

    return (tourney_global_pred[index] >= WT) ? TAKEN : NOTTAKEN;
}

uint8_t tourney_predict_local(uint32_t pc)
{
    uint32_t local_bht_entries = 1 << tourney_lhistoryBits;
    uint32_t pht_index = pc & (local_bht_entries - 1);
    uint32_t index = tourney_local_ht[pht_index] & (local_bht_entries - 1);

    return (tourney_local_pred[index] >= WT) ? TAKEN : NOTTAKEN;
}

uint8_t tourney_predict(uint32_t pc)
{
    uint8_t local_pred = tourney_predict_local(pc);
    uint8_t global_pred = tourney_predict_global(pc);

    uint32_t choice_entries = 1 << tourney_choiceBits;
    uint32_t index = tourney_global_hr & (choice_entries - 1);

    if (isCustom) {
        uint8_t loop_pred = loop_predict(pc);
        if (loop_pred != 0xFF) return loop_pred;
        if (tourney_choice_pred[index] >= WT)
            return local_pred;
        else
            return global_pred;
    } else {
        if (tourney_choice_pred[index] >= WT)
            return local_pred;
        else
            return global_pred;
    }
}

void train_tourney(uint32_t pc, uint8_t outcome)
{
    uint8_t local_pred = tourney_predict_local(pc);
    uint8_t global_pred = tourney_predict_global(pc);

    uint32_t choice_entries = 1 << tourney_choiceBits;
    uint32_t choice_index = tourney_global_hr & (choice_entries - 1);

    if ((local_pred == outcome) && (global_pred != outcome))
    {
        tourney_choice_pred[choice_index] = INC_CNTR(tourney_choice_pred[choice_index]);
    }
    else if ((global_pred == outcome) && (local_pred != outcome))
    {
        tourney_choice_pred[choice_index] = DEC_CNTR(tourney_choice_pred[choice_index]);
    }

    uint32_t global_bht_entries = 1 << tourney_ghistoryBits;
    uint32_t global_index = 0;

    if (isCustom){
        global_index = (pc ^ tourney_global_hr) & (global_bht_entries - 1);
    } else {
        global_index = tourney_global_hr & (global_bht_entries - 1);
    }
    
    uint32_t local_bht_entries = 1 << tourney_lhistoryBits;
    uint32_t pht_index = pc & (local_bht_entries - 1);
    uint32_t local_index = tourney_local_ht[pht_index] & (local_bht_entries - 1);

    if (outcome == TAKEN)
    {
        tourney_global_pred[global_index] = INC_CNTR(tourney_global_pred[global_index]);
        tourney_local_pred[local_index] = INC_CNTR(tourney_local_pred[local_index]);
    }
    else
    {
        tourney_global_pred[global_index] = DEC_CNTR(tourney_global_pred[global_index]);
        tourney_local_pred[local_index] = DEC_CNTR(tourney_local_pred[local_index]);
    }

    if (isCustom)
        train_loop_predictor(pc, outcome);
    
    tourney_global_hr = ((tourney_global_hr << 1) | outcome);
    tourney_local_ht[pht_index] = ((tourney_local_ht[pht_index] << 1) | outcome);
}


// FOR THE CUSTOM PREDICTOR
#define BIMODAL_SIZE 8192       // 8K entries
#define TAGE_MAX_SIZE (1 << 11)

#define c_p_idx_bits 10

#define c_p_idx_size (1 << c_p_idx_bits)
#define c_p_pc_size 16

int8_t W[c_p_idx_size][c_p_pc_size];

#define NUM_TABLES 12

const uint32_t HIST_LENGTHS[NUM_TABLES] = {4, 6, 10, 16, 25, 40, 64, 101, 160, 254, 403, 640};
const uint32_t TAG_WIDTH[NUM_TABLES] = {7, 7, 8, 8, 9, 10, 11, 12, 12, 13, 14, 15};
const uint32_t TABLE_SIZE[NUM_TABLES] =
    {
        10,
        10,
        11,
        11,
        11,
        11,
        11,
        10,
        10,
        10,
        10,
        9
};

typedef struct
{
    uint16_t tag;
    uint8_t ctr;
    uint8_t useful;
} TageEntry;

uint8_t bimodal[BIMODAL_SIZE];
TageEntry tage_tables[NUM_TABLES][TAGE_MAX_SIZE];

uint64_t ghr[10]; // to store 640 bits

uint64_t fold_hash(uint64_t *val, uint32_t bits, uint32_t sz)
{

    uint64_t folded = 0;
    uint32_t i = 0;

    return (val[0] & ((1 << sz) - 1));

    for (i = 0; i < (bits - 1) / 64; i++)
    {
        folded ^= val[i];
    }

    if (i < 10)
    {
        if (bits % 64 == 0)
        {
            folded ^= val[i];
        }
        else
        {
            uint64_t nbits = bits % 64;
            nbits = ((uint64_t)1 << nbits) - 1;
            folded ^= (val[i] & nbits);
        }
    }

    uint64_t final_val = 0;
    for (i = 0; i < 63 / sz; i++)
    {
        uint64_t tmp = (folded >> (i * sz)) & (((uint64_t)1 << sz) - 1);
        final_val ^= tmp;
    }

    uint64_t tmp = (folded >> (i * sz)) & (((uint64_t)1 << (64 % sz)) - 1);
    final_val ^= tmp;

    final_val = final_val % (1 << sz);

    return final_val;
}

void init_custom()
{
    // Initialize bimodal table
    for (int i = 0; i < BIMODAL_SIZE; i++)
    {
        bimodal[i] = WT;
    }

    // Initialize TAGE tables
    for (int t = 0; t < NUM_TABLES; t++)
    {
        for (int i = 0; i < TAGE_MAX_SIZE; i++)
        {
            tage_tables[t][i].tag = 0;
            tage_tables[t][i].ctr = 4; // Weakly taken
            tage_tables[t][i].useful = 0;
        }
    }

    for(int i = 0; i < c_p_idx_size; i++) {
        for(int j = 0; j < c_p_pc_size; j++) {
            W[i][j] = 0;
        }
    }

    // Initialize history
    for (int i = 0; i < 10; i++)
        ghr[i] = 0;
}

static int x = 0;
uint8_t custom_predict(uint32_t pc)
{
    // Base prediction from bimodal

    uint64_t bimodal_idx = pc & ( (1 << c_p_idx_bits ) - 1);
    int64_t y = 0;

    for (int i = 0; i < c_p_pc_size; i ++) {
        y = y + W[bimodal_idx][c_p_pc_size - 1 - i] * ((ghr[i/64] >> i) & 1);
    }

    uint8_t pred = (y > 0) ? TAKEN : NOTTAKEN ;

    int provider = -1;

    // TAGE prediction
    for (int t = NUM_TABLES - 1; t >= 0; t--)
    {
        uint32_t idx = ((pc % (1 << TABLE_SIZE[t])) ^ fold_hash(ghr, HIST_LENGTHS[t], TABLE_SIZE[t]));
        uint16_t tag = ((pc >> 2) ^ (ghr[0] << 4)) & (((uint64_t)1 << TAG_WIDTH[t]) - 1);

        if (tage_tables[t][idx].tag & (((uint64_t)1 << TAG_WIDTH[t]) - 1) == tag)
        {
            if (tage_tables[t][idx].useful != 0) {
                provider = t;
                pred = tage_tables[t][idx].ctr >= 4;
                break;
            }
        }
    }

    return pred;
}

void train_custom(uint32_t pc, uint8_t outcome)
{
    // Update bimodal
    uint64_t bimodal_idx = pc & ( (1 << c_p_idx_bits ) - 1 );
    int64_t y = 0;

    for (int i = 0; i < c_p_pc_size; i ++) {
        y = y + W[bimodal_idx][c_p_pc_size - 1 - i] * ((ghr[i/64] >> i) & 1);
    }

    int corr = (outcome == TAKEN) ? 1 : -1;
    if( ((y < 0) != (corr < 0)) || (y < c_p_pc_size/2 && y > -c_p_pc_size/2) )
    {
        for(int i = 0; i < c_p_pc_size; i++)
        {
            W[bimodal_idx][c_p_pc_size - 1 - i] += ((ghr[i/64] >> i) & 1) * corr;
        }           
    }

    // TAGE update
    int provider = -1;
    int alloc_table = -1;

    uint8_t tage_pred = custom_predict(pc);

    // Find provider and potential allocation candidates
    for (int t = NUM_TABLES - 1; t >= 0; t--)
    {
        uint32_t idx = ((pc % (1 << TABLE_SIZE[t])) ^ fold_hash(ghr, HIST_LENGTHS[t], TABLE_SIZE[t]));
        uint16_t tag = ((pc >> 2) ^ (ghr[0] << 4)) & (((uint64_t)1 << TAG_WIDTH[t]) - 1);

        if (tage_tables[t][idx].tag == (tag & (((uint64_t)1 << TAG_WIDTH[t]) - 1)))
        {
            if (provider == -1)
            {
                provider = t;
                // Update counter
                if (outcome)
                {
                    tage_tables[t][idx].ctr = INC_3B_CNTR(tage_tables[t][idx].ctr);
                }
                else
                {
                    tage_tables[t][idx].ctr = DEC_3B_CNTR(tage_tables[t][idx].ctr);
                }
                if (outcome == tage_pred)
                {
                    tage_tables[t][idx].useful = INC_CNTR(tage_tables[t][idx].useful);
                    tage_tables[t][idx].useful = INC_CNTR(tage_tables[t][idx].useful);
                }
            }
            // Update usefulness
            tage_tables[t][idx].useful = DEC_CNTR(tage_tables[t][idx].useful);
        }
        else if (t < alloc_table && tage_tables[t][idx].useful == 0)
        {
            alloc_table = t;
        }
    }

    uint32_t p_idx = provider != -1 ? ((pc % (1 << TABLE_SIZE[provider])) ^ fold_hash(ghr, HIST_LENGTHS[provider], TABLE_SIZE[provider])) : -1;

    // Allocation logic
    if ((provider == -1) || (tage_tables[provider][p_idx].ctr >= 4) != outcome)
    {
        // if (bimodal_update) {
        if (alloc_table == -1)
        {
            // Find table with least useful entries
            uint8_t min_useful = 7;
            for (int t = 0; t < NUM_TABLES; t++)
            {
                uint32_t idx = ((pc % (1 << TABLE_SIZE[t])) ^ fold_hash(ghr, HIST_LENGTHS[t], TABLE_SIZE[t]));
                if (tage_tables[t][idx].useful < min_useful)
                {
                    min_useful = tage_tables[t][idx].useful;
                    alloc_table = t;
                }
            }
        }

        if (alloc_table != -1)
        {
            uint32_t idx = ((pc % (1 << TABLE_SIZE[alloc_table])) ^ fold_hash(ghr, HIST_LENGTHS[alloc_table], TABLE_SIZE[alloc_table]));
            tage_tables[alloc_table][idx].tag = ((pc >> 2) ^ (ghr[0] << 4)) & (((uint64_t)1 << TAG_WIDTH[alloc_table]) - 1);
            tage_tables[alloc_table][idx].ctr = outcome ? 6 : 2;
            tage_tables[alloc_table][idx].useful = 3;
        }
        // }
    }
    // Update history
    for (int i = 10; i > 0; i--)
    {
        uint8_t x = (ghr[i - 1] >> 63) & 1;
        ghr[i] = (ghr[i] << 1) | x;
    }
    ghr[0] = (ghr[0] << 1) | outcome;
}

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

void init_predictor()
{
    switch (bpType)
    {
    case STATIC:
        break;
    case GSHARE:
        init_gshare();
        break;
    case TOURNAMENT:
        init_tourney();
        break;
    case CUSTOM:
        isCustom = true;
        init_tourney();
        break;
    default:
        break;
    }
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint32_t make_prediction(uint32_t pc, uint32_t target, uint32_t direct)
{

    // Make a prediction based on the bpType
    switch (bpType)
    {
    case STATIC:
        return TAKEN;
    case GSHARE:
        return gshare_predict(pc);
    case TOURNAMENT:
        return tourney_predict(pc);
    case CUSTOM:
        return tourney_predict(pc);
    default:
        break;
    }

    // If there is not a compatable bpType then return NOTTAKEN
    return NOTTAKEN;
}

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//

void train_predictor(uint32_t pc, uint32_t target, uint32_t outcome, uint32_t condition, uint32_t call, uint32_t ret, uint32_t direct)
{
    if (condition)
    {
        switch (bpType)
        {
        case STATIC:
            return;
        case GSHARE:
            return train_gshare(pc, outcome);
        case TOURNAMENT:
            return train_tourney(pc, outcome);
        case CUSTOM:
            return train_tourney(pc, outcome);
        default:
            break;
        }
    }
}
