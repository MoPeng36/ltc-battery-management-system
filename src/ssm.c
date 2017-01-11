#include "ssm.h"
#include "charge.h"
#include "discharge.h"
#include "error.h"

static void Load_EEPROM_PackConfig(PACK_CONFIG_T *pack_config);
static void Check_PackConfig_With_LTC(PACK_CONFIG_T *pack_config);
static bool Is_Valid_Jump(BMS_SSM_MODE_T mode1, BMS_SSM_MODE_T mode2);
static bool Is_State_Done(BMS_STATE_T *state);

void SSM_Init(BMS_STATE_T *state) {
    // Initialize BMS state variables
    state->curr_mode = BMS_SSM_MODE_INIT;
    state->init_state = BMS_INIT_RUNNING;
    state->charge_state = BMS_CHARGE_OFF;
    state->discharge_state = BMS_DISCHARGE_OFF;
    state->error_code = BMS_NO_ERROR;
}

void Init_Step(BMS_INPUT_T *input, BMS_STATE_T *state, BMS_OUTPUT_T *output) {
    Load_EEPROM_PackConfig(state->pack_config);
    Check_PackConfig_With_LTC(state->pack_config);
    state->curr_mode = BMS_SSM_MODE_STANDBY;
}

static void Load_EEPROM_PackConfig(PACK_CONFIG_T *pack_config) {
    // Load pack configuration from EEPROM
}

static void Check_PackConfig_With_LTC(PACK_CONFIG_T *pack_config) {
    // Send/recieve SPI messages to make sure *pack_config is correct
}


static bool Is_Valid_Jump(BMS_SSM_MODE_T mode1, BMS_SSM_MODE_T mode2) {
    if(mode1 == BMS_SSM_MODE_STANDBY && mode2 == BMS_SSM_MODE_CHARGE) {
        return true;
    } else if(mode1 == BMS_SSM_MODE_STANDBY && mode2 == BMS_SSM_MODE_BALANCE) {
        return true;
    } else if(mode1 == BMS_SSM_MODE_STANDBY && mode2 == BMS_SSM_MODE_DISCHARGE) {
        return true;
    } else if(mode1 == BMS_SSM_MODE_CHARGE && mode2 == BMS_SSM_MODE_STANDBY) {
        return true;
    } else if(mode1 == BMS_SSM_MODE_DISCHARGE && mode2 == BMS_SSM_MODE_STANDBY) {
        return true;
    } else if(mode1 == BMS_SSM_MODE_BALANCE && mode2 == BMS_SSM_MODE_STANDBY) {
        return true;
    }
    return false;
}

static bool Is_State_Done(BMS_STATE_T *state) {
    switch(state->curr_mode) {
        case BMS_SSM_MODE_CHARGE:
            return state->charge_state == BMS_CHARGE_DONE;
        case BMS_SSM_MODE_DISCHARGE:
            return state->discharge_state == BMS_DISCHARGE_DONE;
        case BMS_SSM_MODE_INIT:
            return state->init_state == BMS_INIT_DONE;
        case BMS_SSM_MODE_ERROR:
            return false;
        default:
            return true;
    }
}

void SSM_Step(BMS_INPUT_T *input, BMS_STATE_T *state, BMS_OUTPUT_T *output) {
    // OUTLINE:
    // If change state request made and possible, change state
    // Check if in standby:
    //     if in standby:
    //          check if config commands in config buffer currently
    //          (if so write those to EEPROM)
    //          if mode request change valid, switch over
    //     else dispatch step to appropriate SM step
    
    if(Is_Valid_Jump(state->curr_mode, input->mode_request) 
            && Is_State_Done(state)) {
        state->curr_mode = input->mode_request;
    }

    switch(state->curr_mode) {
        case BMS_SSM_MODE_STANDBY:
            break;
        case BMS_SSM_MODE_INIT:
            Init_Step(input, state, output);
            break;
        case BMS_SSM_MODE_CHARGE:
            Charge_Step(input, state, output);
            break;
        case BMS_SSM_MODE_DISCHARGE:
            Discharge_Step(input, state, output);
            break;
        case BMS_SSM_MODE_ERROR:
            Error_Step(input, state, output);
            break;
        case BMS_SSM_MODE_BALANCE:
            //TODO
            break;
    }
}
