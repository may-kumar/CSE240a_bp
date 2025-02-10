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
// TODO:Student Information
//
const char *studentName = "TODO";
const char *studentID = "TODO";
const char *email = "TODO";

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



//Tourney Predictor :
// tourament functions
//21264 Alpha processor : tournament predictor

// t = table
// r = register
uint32_t tourney_lhistoryBits = 10; // Number of bits used for Local History
uint32_t tourney_ghistoryBits = 16; // Number of bits used for Global History
uint32_t tourney_choiceBits   = 16; // Number of bits used for Choice Table
uint32_t *tourney_local_ht;
uint32_t tourney_global_hr;

uint8_t *tourney_local_pred;
uint8_t *tourney_global_pred;
uint8_t *tourney_choice_pred;


void init_tourney()
{
  int local_bht_entries = 1 << tourney_lhistoryBits;
  tourney_local_pred = (uint8_t *)malloc(local_bht_entries * sizeof(uint8_t));
  tourney_local_ht = (uint32_t *)malloc(local_bht_entries * sizeof(uint32_t));
  int i = 0;
  for (i = 0; i < local_bht_entries; i++)
  {
    tourney_local_pred[i] = WT;
    tourney_local_ht[i] = 0;
  }

  int global_bht_entries = 1 << tourney_ghistoryBits;
  tourney_global_pred = (uint8_t *)malloc(global_bht_entries * sizeof(uint8_t));
  for (i = 0; i < global_bht_entries; i++)
  {
    tourney_global_pred[i] = WT;
  }

  int choice_t_entries = 1 << tourney_choiceBits;
  tourney_choice_pred = (uint8_t *)malloc(choice_t_entries * sizeof(uint8_t));
  for (i = 0; i < choice_t_entries; i++)
  {
    tourney_choice_pred[i] = WT;
  }
}


uint8_t tourney_predict_global(uint32_t pc) {
    uint32_t global_bht_entries = 1 << tourney_ghistoryBits;
    uint32_t index = tourney_global_hr &  (global_bht_entries - 1);

    return tourney_global_pred[index] >= 2;
}

uint8_t tourney_predict_local(uint32_t pc) {
    uint32_t local_bht_entries = 1 << tourney_lhistoryBits;
    uint32_t pht_index = pc & (local_bht_entries - 1);
    uint32_t index = tourney_local_ht[pht_index];

    return tourney_local_pred[index] >= 2;
}

uint8_t tourney_predict(uint32_t pc) {
    uint8_t local_pred = tourney_predict_local(pc);
    uint8_t global_pred = tourney_predict_global(pc);

    uint32_t choice_entries = 1 << tourney_choiceBits;
    uint32_t index = tourney_global_hr & (choice_entries - 1);

    if (tourney_choice_pred[index] >= 2) return local_pred;
    else return global_pred;
}


void train_tourney(uint32_t pc, uint8_t outcome) {
    uint8_t local_pred = tourney_predict_local(pc);
    uint8_t global_pred = tourney_predict_global(pc);

    uint32_t choice_entries = 1 << tourney_choiceBits;
    uint32_t choice_index = tourney_global_hr & (choice_entries - 1);

    if ( (local_pred == outcome) && (global_pred != outcome) ) {
        tourney_choice_pred[choice_index] = INC_CNTR(tourney_choice_pred[choice_index]);
    } else if ( (global_pred == outcome) && (local_pred != outcome) ) {
        tourney_choice_pred[choice_index] = DEC_CNTR(tourney_choice_pred[choice_index]);
    }

    uint32_t global_bht_entries = 1 << tourney_ghistoryBits;
    uint32_t global_index = tourney_global_hr & (global_bht_entries - 1);

    uint32_t local_bht_entries = 1 << tourney_lhistoryBits;
    uint32_t pht_index = pc & (local_bht_entries - 1);
    uint32_t local_index = tourney_local_ht[pht_index];

    if (outcome == TAKEN) {
        tourney_global_pred[global_index] = INC_CNTR(tourney_global_pred[global_index]);
        tourney_local_pred[local_index] = INC_CNTR(tourney_local_pred[local_index]);
    } else {
        tourney_global_pred[global_index] = DEC_CNTR(tourney_global_pred[global_index]);
        tourney_local_pred[local_index] = DEC_CNTR(tourney_local_pred[local_index]);
    }

    tourney_global_hr = ( (tourney_global_hr << 1) | outcome ) & (global_bht_entries - 1);
    tourney_local_ht[pht_index] = ( (tourney_local_ht[pht_index] << 1) | outcome ) & (local_bht_entries - 1);
}




// custom Predictor :
// Modified 21264 Alpha processor : tournament predictor

// t = table
// r = register
#define custom_lhistoryBits 10    // Number of bits used for Local Pattern History
#define custom_lbhthistoryBits 16 // Number of bits used for Local BHT
#define custom_ghistoryBits 12    // Number of bits used for Global History
#define custom_choiceBits 12      // Number of bits used for Choice Table

uint64_t custom_global_hr;

uint16_t *custom_local_ht;
uint8_t *custom_local_pred;
uint8_t *custom_global_pred;
uint8_t *custom_choice_pred;

void init_custom()
{
  int local_bht_entries = 1 << custom_lhistoryBits;
  custom_local_ht = (uint16_t *) malloc (sizeof(uint16_t) * local_bht_entries);
  custom_local_pred = (uint8_t *) malloc (sizeof(uint8_t) * local_bht_entries);

  for (int i = 0; i < local_bht_entries; i++)
  {
    custom_local_ht[i] = 0;
  }

  for (int i = 0; i < local_bht_entries; i++)
  {
    custom_local_pred[i] = WN;
  }

  int global_bht_entries = 1 << custom_ghistoryBits;
  custom_global_pred = (uint8_t *) malloc (sizeof(uint8_t) * global_bht_entries);
  for (int i = 0; i < global_bht_entries; i++)
  {
    custom_global_pred[i] = WN;
  }

  int choice_t_entries = 1 << custom_choiceBits;
  custom_choice_pred = (uint8_t *) malloc (sizeof(uint8_t) * choice_t_entries);
  for (int i = 0; i < choice_t_entries; i++)
  {
    custom_choice_pred[i] = WN;
  }

  custom_global_hr = 0;
}



uint8_t custom_predict_global(uint32_t pc)
{
    uint16_t global_index = custom_global_hr & ((1 << custom_ghistoryBits) - 1);
    return (custom_global_pred[global_index] >= WT) ? TAKEN : NOTTAKEN;
}

uint8_t custom_predict_local(uint32_t pc)
{
    uint16_t pht_index = pc & ((1 << custom_lhistoryBits) - 1);
    uint16_t local_index = custom_local_ht[pht_index];
    // printf("%0X\n", local_index);
    return (custom_local_pred[local_index] >= WT) ? TAKEN : NOTTAKEN;
}

uint8_t custom_predict(uint32_t pc)
{
    uint16_t choice_index = custom_global_hr & ((1 << custom_ghistoryBits) - 1);
    return (custom_choice_pred[choice_index] >= WT) ? custom_predict_global(pc) : custom_predict_local(pc);
}



void train_custom(uint32_t pc, uint8_t outcome)
{
  uint8_t local_pred  = custom_predict_local(pc);
  uint8_t global_pred = custom_predict_global(pc);

  
  uint16_t pht_index = pc & ((1 << custom_lhistoryBits) - 1);
  uint16_t local_index = custom_local_ht[pht_index];
  custom_local_pred[local_index] = (outcome == TAKEN) ? INC_CNTR(custom_local_pred[local_index]) : DEC_CNTR(custom_local_pred[local_index]);

  
  uint16_t global_index = custom_global_hr & ((1 << custom_ghistoryBits) - 1);
  custom_global_pred[global_index] = (outcome == TAKEN) ? INC_CNTR(custom_global_pred[global_index]) : DEC_CNTR(custom_global_pred[global_index]);


  uint16_t choice_index = custom_global_hr & ((1 << custom_ghistoryBits) - 1);
  if (global_pred == outcome && local_pred != outcome)
  {
    custom_choice_pred[choice_index] = INC_CNTR(custom_choice_pred[choice_index]);
  }
  else if (global_pred != outcome && local_pred == outcome)
  {
    custom_choice_pred[choice_index] = DEC_CNTR(custom_choice_pred[choice_index]);
  }

  custom_global_hr = (custom_global_hr << 1) | outcome;
  custom_local_ht[pht_index] = (custom_local_ht[pht_index] << 1) | outcome;

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
        init_custom();
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
        return custom_predict(pc);
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
            return train_custom(pc, outcome);
        default:
            break;
        }
    }
}
