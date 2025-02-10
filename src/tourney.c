
// Tourney Predictor :
//  tourament functions

// 21264 Alpha processor : tournament predictor

// t = table
// r = register
#define tourney_lhistoryBits 10    // Number of bits used for Local Pattern History
#define tourney_lbhthistoryBits 16 // Number of bits used for Local BHT
#define tourney_ghistoryBits 16    // Number of bits used for Global History
#define tourney_choiceBits 16      // Number of bits used for Choice Table

uint64_t tourney_global_hr;

uint16_t *tourney_local_ht;
uint8_t *tourney_local_pred;
uint8_t *tourney_global_pred;
uint8_t *tourney_choice_pred;

void init_tourney()
{
  int local_bht_entries = 1 << tourney_lhistoryBits;
  tourney_local_ht = (uint16_t *) malloc (sizeof(uint16_t) * local_bht_entries);
  tourney_local_pred = (uint8_t *) malloc (sizeof(uint8_t) * local_bht_entries);

  for (int i = 0; i < local_bht_entries; i++)
  {
    tourney_local_ht[i] = 0;
  }

  for (int i = 0; i < local_bht_entries; i++)
  {
    tourney_local_pred[i] = WN;
  }

  int global_bht_entries = 1 << tourney_ghistoryBits;
  tourney_global_pred = (uint8_t *) malloc (sizeof(uint8_t) * global_bht_entries);
  for (int i = 0; i < global_bht_entries; i++)
  {
    tourney_global_pred[i] = WN;
  }

  int choice_t_entries = 1 << tourney_choiceBits;
  tourney_choice_pred = (uint8_t *) malloc (sizeof(uint8_t) * choice_t_entries);
  for (int i = 0; i < choice_t_entries; i++)
  {
    tourney_choice_pred[i] = WN;
  }

  tourney_global_hr = 0;
}



uint8_t tourney_predict_global(uint32_t pc)
{
    uint16_t global_index = tourney_global_hr & ((1 << tourney_ghistoryBits) - 1);
    return (tourney_global_pred[global_index] >= WT) ? TAKEN : NOTTAKEN;
}

uint8_t tourney_predict_local(uint32_t pc)
{
    uint16_t pht_index = pc & ((1 << tourney_lhistoryBits) - 1);
    uint16_t local_index = tourney_local_ht[pht_index];
    return (tourney_local_pred[local_index] >= WT) ? TAKEN : NOTTAKEN;
}

uint8_t tourney_predict(uint32_t pc)
{
    uint16_t choice_index = tourney_global_hr & ((1 << tourney_ghistoryBits) - 1);
    return (tourney_choice_pred[choice_index] >= WT) ? tourney_predict_global(pc) : tourney_predict_local(pc);
}



void train_tourney(uint32_t pc, uint8_t outcome)
{
  uint8_t local_pred  = tourney_predict_local(pc);
  uint8_t global_pred = tourney_predict_global(pc);

  
  uint16_t pht_index = pc & ((1 << tourney_lhistoryBits) - 1);
  uint16_t local_index = tourney_local_ht[pht_index];
  tourney_local_pred[local_index] = (outcome == TAKEN) ? INC_CNTR(tourney_local_pred[local_index]) : DEC_CNTR(tourney_local_pred[local_index]);

  
  uint16_t global_index = tourney_global_hr & ((1 << tourney_ghistoryBits) - 1);
  tourney_global_pred[global_index] = (outcome == TAKEN) ? INC_CNTR(tourney_global_pred[global_index]) : DEC_CNTR(tourney_global_pred[global_index]);


  uint16_t choice_index = tourney_global_hr & ((1 << tourney_ghistoryBits) - 1);
  if (global_pred == outcome && local_pred != outcome)
  {
    tourney_choice_pred[choice_index] = INC_CNTR(tourney_choice_pred[choice_index]);
  }
  else if (global_pred != outcome && local_pred == outcome)
  {
    tourney_choice_pred[choice_index] = DEC_CNTR(tourney_choice_pred[choice_index]);
  }

  tourney_global_hr = (tourney_global_hr << 1) | outcome;
  tourney_local_ht[pht_index] = (tourney_local_ht[pht_index] << 1) | outcome;

}