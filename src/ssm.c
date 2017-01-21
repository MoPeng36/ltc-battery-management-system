#include "ssm.h"
#include "charge.h"
#include "discharge.h"
#include "error.h"
#include "config.h"
#include "bms_utils.h"

void SSM_Init(BMS_INPUT_T *input, BMS_STATE_T *state, BMS_OUTPUT_T *output) {
    // Initialize BMS state variables
    state->curr_mode = BMS_SSM_MODE_INIT;
    state->init_state = BMS_INIT_OFF;
    state->error_code = BMS_NO_ERROR;

    output->read_eeprom_packconfig = false;
    output->check_packconfig_with_ltc = false;

    input->ltc_packconfig_check_done = false;
    input->eeprom_packconfig_read_done = false;

    input->eeprom_read_error = false;
    input->ltc_error = LTC_NO_ERROR;

	Charge_Init(state);
	Discharge_Init(state);
}

BMS_ERROR_T Init_Step(BMS_INPUT_T *input, BMS_STATE_T *state, BMS_OUTPUT_T *output) {
    switch(state->init_state) {
        case(BMS_INIT_OFF):
            output->read_eeprom_packconfig = true;
            state->init_state = BMS_INIT_READ_PACKCONFIG;
            input->eeprom_packconfig_read_done = false;
            break;
        case(BMS_INIT_READ_PACKCONFIG):
            if(input->eeprom_packconfig_read_done) {
                output->read_eeprom_packconfig = false;
                output->check_packconfig_with_ltc = true;
                state->init_state = BMS_INIT_CHECK_PACKCONFIG;
                input->ltc_packconfig_check_done = false;
                input->eeprom_packconfig_read_done = false;
            }
            break;
        case(BMS_INIT_CHECK_PACKCONFIG):
            if(input->ltc_packconfig_check_done) {
                output->check_packconfig_with_ltc = false;
                state->init_state = BMS_INIT_DONE;
                state->curr_mode = BMS_SSM_MODE_STANDBY;
                input->ltc_packconfig_check_done = false;
            }
            break;
        case(BMS_INIT_DONE):
            state->curr_mode = BMS_SSM_MODE_STANDBY;
            break;
    }
    return 0;
}

bool Is_Valid_Jump(BMS_SSM_MODE_T mode1, BMS_SSM_MODE_T mode2) {

    if(mode1 == BMS_SSM_MODE_STANDBY && mode2 == BMS_SSM_MODE_CHARGE) {
        return true;
    } else if(mode1 == BMS_SSM_MODE_STANDBY && mode2 == BMS_SSM_MODE_BALANCE) {
        return true;
    } else if(mode1 == BMS_SSM_MODE_STANDBY && mode2 == BMS_SSM_MODE_DISCHARGE) {
        return true;

    } else if(mode1 == BMS_SSM_MODE_CHARGE && mode2 == BMS_SSM_MODE_STANDBY) {
        return true;
    } else if(mode1 == BMS_SSM_MODE_BALANCE && mode2 == BMS_SSM_MODE_STANDBY) {
        return true;
    } else if(mode1 == BMS_SSM_MODE_DISCHARGE && mode2 == BMS_SSM_MODE_STANDBY) {
        return true;
    } else if(mode1 == BMS_SSM_MODE_BALANCE && mode2 == BMS_SSM_MODE_STANDBY) {
        return true;

    } else if(mode1 == BMS_SSM_MODE_BALANCE && mode2 == BMS_SSM_MODE_CHARGE) {
        return true;
    } else if(mode1 == BMS_SSM_MODE_CHARGE && mode2 == BMS_SSM_MODE_BALANCE) {
        return true;
    }
    return false;
}

bool Is_Charge_Balance_Switch(BMS_SSM_MODE_T mode1, BMS_SSM_MODE_T mode2) {
    return (mode1 == BMS_SSM_MODE_BALANCE && mode2 == BMS_SSM_MODE_CHARGE)
        || (mode1 == BMS_SSM_MODE_CHARGE && mode2 == BMS_SSM_MODE_BALANCE);
}

bool Is_State_Done(BMS_STATE_T *state) {
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

BMS_ERROR_T Error_Step(BMS_INPUT_T *input, BMS_STATE_T *state, BMS_OUTPUT_T *output) {
    output->close_contactors = false;
    output->charge_req->charger_on = false;
	memset(output->balance_req, 0, sizeof(output->balance_req[0])*Get_Total_Cell_Count(state->pack_config));
    output->read_eeprom_packconfig = false;
    output->check_packconfig_with_ltc = false;
    return 0;
}

void Check_Error(BMS_INPUT_T *input, BMS_STATE_T *state, BMS_OUTPUT_T *output) {
    // checks if there is a reported error
    //    communicating with the eeprom or ltc
    if(state->curr_mode == BMS_SSM_MODE_ERROR) return;
    if(input->eeprom_read_error) {
        state->curr_mode = BMS_SSM_MODE_ERROR;
        state->error_code = BMS_EEPROM_ERROR;
        return;
    }
    if(input->ltc_error != LTC_NO_ERROR) {
        state->curr_mode = BMS_SSM_MODE_ERROR;
        state->error_code = BMS_LTC_ERROR;
        return;
    }

    uint32_t max_cell_temp_thres_C = state->pack_config->max_cell_temp_C;
    if(input->pack_status->max_cell_temp_C >= max_cell_temp_thres_C) {
        state->curr_mode = BMS_SSM_MODE_ERROR;
        state->error_code = BMS_CELL_OVER_TEMP;
        return;
    }
}


void SSM_Step(BMS_INPUT_T *input, BMS_STATE_T *state, BMS_OUTPUT_T *output) {
    // OUTLINE:
    // If change state request made and possible, change state
    // Check if in standby:
    //     if in standby:
    //          if mode request change valid, switch over
    //     else dispatch step to appropriate SM step
    Check_Error(input, state, output);
    
    if((Is_Valid_Jump(state->curr_mode, input->mode_request)
            && Is_State_Done(state))
            || Is_Charge_Balance_Switch(state->curr_mode, input->mode_request)) {
        state->curr_mode = input->mode_request;
    }

    BMS_ERROR_T err = 0;
    switch(state->curr_mode) {
        case BMS_SSM_MODE_STANDBY:
            break;
        case BMS_SSM_MODE_INIT:
            err = Init_Step(input, state, output);
            break;
        case BMS_SSM_MODE_CHARGE:
            err = Charge_Step(input, state, output);
            break;
        case BMS_SSM_MODE_DISCHARGE:
            err = Discharge_Step(input, state, output);
            break;
        case BMS_SSM_MODE_ERROR:
            err = Error_Step(input, state, output);
            break;
        case BMS_SSM_MODE_BALANCE:
            err = Charge_Step(input, state, output);
            break;
    }

    if(err) {
        state->curr_mode = BMS_SSM_MODE_ERROR;
        state->error_code = err;
    }
}