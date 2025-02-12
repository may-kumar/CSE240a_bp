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

uint32_t branch_count = 0;

// Tourney Predictor :
//  tourament functions

#define LTB_ENTRIES 64  // Loop Termination Buffer entries
#define MAX_LOOP_COUNT ((1 << 16) - 1)
#define CONF_THRESH 6    // Confidence threshold for stable prediction

typedef struct {
    uint32_t tag;           // Partial PC signature
    uint16_t max_count;     // Predicted loop trip count
    uint16_t current_count; // Current iteration count
    uint8_t confidence;     // Confidence counter (0-7)
    uint8_t valid;          // Valid entry flag
} LTB_Entry;

LTB_Entry ltb[LTB_ENTRIES][2];

// Initialize Loop Termination Buffer
void init_loop_predictor() {
    return;
}

// Loop prediction logic
uint8_t loop_predict(uint32_t pc) {

    uint32_t index = pc % LTB_ENTRIES;
    LTB_Entry *entry0 = &ltb[index][0];
    LTB_Entry *entry1 = &ltb[index][1];
    
    // Tag match check
    if (entry0->valid && entry0->tag == (((pc << 7) ^ (pc >> 9)) & 0xFFF)) {
        if (entry0->confidence >= CONF_THRESH) {
            return (entry0->current_count < entry0->max_count) ? TAKEN : NOTTAKEN;
        }
    }
    // Tag match check
    if (entry1->valid && entry1->tag == (((pc << 7) ^ (pc >> 9)) & 0xFFF)) {
        if (entry1->confidence >= CONF_THRESH) {
            return (entry1->current_count < entry1->max_count) ? TAKEN : NOTTAKEN;
        }
    }
    
    // Default prediction for undetected loops
    return 0xFF;
}

// Train loop predictor
void train_loop_predictor(uint32_t pc, uint8_t outcome) {

    uint32_t index = pc % LTB_ENTRIES;
    LTB_Entry *entry0 = &ltb[index][0];
    LTB_Entry *entry1 = &ltb[index][1];
    
    // Detect new loops or update existing entries
    if (entry0->valid && entry0->tag == (((pc << 7) ^ (pc >> 9)) & 0xFFF)) {
        if (outcome == TAKEN) {
            entry0->current_count++;
        } else {
            // Loop exit - update confidence and max_count
            if (entry0->current_count == entry0->max_count) {
                entry0->confidence = (entry0->confidence < 7) ? entry0->confidence + 1 : 7;
            } else {
                entry0->max_count = entry0->current_count;
                entry0->confidence = 0;
            }
            entry0->current_count = 0;
        }
    } else if (entry1->valid && entry1->tag == (((pc << 7) ^ (pc >> 9)) & 0xFFF)) {
        if (outcome == TAKEN) {
            entry1->current_count++;
        } else {
            // Loop exit - update confidence and max_count
            if (entry1->current_count == entry1->max_count) {
                entry1->confidence = (entry1->confidence < 7) ? entry1->confidence + 1 : 7;
            } else {
                entry1->max_count = entry1->current_count;
                entry1->confidence = 0;
            }
            entry1->current_count = 0;
        }
    } else {
        LTB_Entry *replace = NULL;
        if (!entry0->valid || entry0->confidence < 3) replace = entry0;
        else if (!entry1->valid || entry1->confidence < 3) replace = entry1;
        else {
            entry0->confidence = entry0->confidence << 1;
            entry1->confidence = entry1->confidence << 1;
        }
        // Allocate new entry on first loop iteration
        if (replace != NULL && outcome == TAKEN) {
            replace->tag = ((pc << 7) ^ (pc >> 9)) & 0xFFF;
            replace->valid = 1;
            replace->current_count = 1;
            replace->max_count = MAX_LOOP_COUNT;
            replace->confidence = 0;
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
    if (isCustom) {
        uint8_t loop_pred = loop_predict(pc);
        if (loop_pred != 0xFF) return loop_pred;
    }

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

    if (tourney_choice_pred[index] >= WT)
        return local_pred;
    else
        return global_pred;
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
