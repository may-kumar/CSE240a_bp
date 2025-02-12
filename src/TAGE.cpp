
// FOR THE CUSTOM PREDICTOR
#define BIMODAL_SIZE 8192       // 8K entries
#define TAGE_MAX_SIZE (1 << 16)

#define NUM_TABLES 5

const uint32_t HIST_LENGTHS[NUM_TABLES] = {4, 8, 16, 32, 64};
const uint32_t TAG_WIDTH[NUM_TABLES] = {10, 10, 10, 10, 10};
const uint32_t TABLE_SIZE[NUM_TABLES] =
    {
        16,
        16,
        16,
        16,
        16
};

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

typedef struct
{
    uint16_t tag;
    uint8_t ctr;
    uint8_t useful;
} TageEntry;

uint8_t bimodal[BIMODAL_SIZE];
TageEntry tage_tables[NUM_TABLES][TAGE_MAX_SIZE];

uint64_t ghr[10]; // to store 640 bits


uint64_t fold_hash(uint64_t pc, uint64_t *val, uint32_t history_length, uint32_t output_bits)
{

    uint32_t folded = 0;
    uint32_t chunks = (history_length + 63) / 64;
    
    for(int i = 0; i < chunks; i++) {
        folded ^= (val[i] & ((1ULL << (history_length % 64)) - 1));
        history_length = history_length > 64 ? history_length - 64 : 0;
    }
    
    // Final compression
    folded ^= folded >> (output_bits / 2);
    folded ^= folded >> (output_bits / 4);
    return (pc ^ folded) & ((1 << output_bits) - 1);
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

    // Initialize history
    for (int i = 0; i < 10; i++)
        ghr[i] = 0;

}

static int x = 0;
uint8_t custom_predict(uint32_t pc)
{
    // Base prediction from bimodal
    uint32_t bimodal_idx = pc % BIMODAL_SIZE;
    uint8_t pred = bimodal[bimodal_idx] >= WT;
    int provider = -1;
    int alt_prov = -1;

    // TAGE prediction
    for (int t = NUM_TABLES - 1; t >= 0; t--)
    {
        uint32_t idx = (fold_hash(pc, ghr, HIST_LENGTHS[t], TABLE_SIZE[t]));
        uint16_t tag = ((pc >> 2) ^ (ghr[0] << 4)) & (((uint64_t)1 << TAG_WIDTH[t]) - 1);;

        if (tage_tables[t][idx].tag == tag)
        {
            if (provider == -1) {
                provider = t;
                pred = tage_tables[t][idx].ctr >= WT3;
                if (tage_tables[t][idx].useful >= 2) break;
            } else if (alt_prov == -1) {
                alt_prov = t;
                pred = tage_tables[t][idx].ctr >= WT3;
            }
        }
    }

    return pred;
}

void train_custom(uint32_t pc, uint8_t outcome)
{
    // Update bimodal
    uint32_t bimodal_idx = pc % BIMODAL_SIZE;


    if (outcome)
    {
        bimodal[bimodal_idx] = INC_CNTR(bimodal[bimodal_idx]);
    }
    else
    {
        bimodal[bimodal_idx] = DEC_CNTR(bimodal[bimodal_idx]);
    }

    uint8_t bimodal_pred = bimodal[bimodal_idx];

    // TAGE update
    int provider = -1;
    int alt_prov = -1;

    uint8_t provPred = -1;
    uint8_t altPred = -1;

    uint32_t prov_idx = -1;
    uint32_t alt_idx = -1;

    int alloc_table = -1;

    // Find provider and potential allocation candidates
    for (int t = NUM_TABLES - 1; t >= 0; t--)
    {
        uint32_t idx = (fold_hash(pc, ghr, HIST_LENGTHS[t], TABLE_SIZE[t]));
        uint16_t tag = ((pc >> 2) ^ (ghr[0] << 4)) & (((uint64_t)1 << TAG_WIDTH[t]) - 1);

        if (tage_tables[t][idx].tag == tag)
        {
            if (provider == -1)
            {
                provider = t;
                provPred = (tage_tables[t][idx].ctr >= WT3);
                prov_idx = idx;
            } else if (alt_prov == -1) {
                alt_prov = t;
                altPred = (tage_tables[t][idx].ctr >= WT3);
                alt_idx = idx;
            } 
            
            if (alloc_table == -1 && tage_tables[t][idx].useful == 0 && t > provider) {
                alloc_table = t;
            }
        }
    }

    if (provider != -1) {
        if (provPred == outcome) {
            if (outcome) {
                tage_tables[provider][prov_idx].ctr = INC_3B_CNTR(tage_tables[provider][prov_idx].ctr);
            } else {
                tage_tables[provider][prov_idx].ctr = DEC_3B_CNTR(tage_tables[provider][prov_idx].ctr);
            }
        }

        if (alt_prov != -1) {
            if (provPred == outcome && altPred != outcome) {
                tage_tables[provider][prov_idx].useful = INC_CNTR(tage_tables[provider][prov_idx].useful);
            } else if (provPred != outcome && altPred == outcome) {
                tage_tables[provider][prov_idx].useful = DEC_CNTR(tage_tables[provider][prov_idx].useful);
            }
        }

        // Allocation logic
        if (outcome != provPred) {
            if (alloc_table != -1) {
                uint32_t idx = (fold_hash(pc, ghr, HIST_LENGTHS[alloc_table], TABLE_SIZE[alloc_table]));
                tage_tables[alloc_table][idx].tag = ((pc >> 2) ^ (ghr[0] << 4)) & (((uint64_t)1 << TAG_WIDTH[alloc_table]) - 1);
                tage_tables[alloc_table][idx].ctr = outcome ? 6 : 2;
                tage_tables[alloc_table][idx].useful = 0;
            } else {
                for (int j = provider+1; j < NUM_TABLES; j++) {
                    uint32_t idx = (fold_hash(pc, ghr, HIST_LENGTHS[j], TABLE_SIZE[j]));
                    tage_tables[j][idx].useful = DEC_CNTR(tage_tables[j][idx].useful);
                }
            }
        }
    }

    // Update history
    for (int i = 10; i > 0; i--) {
        uint8_t x = (ghr[i-1] >> 63) & 1;
        ghr[i] = (ghr[i] << 1) | x;
    }
    ghr[0] = (ghr[0] << 1) | outcome;
}
