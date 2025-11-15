#include <stdint.h>
#include <math.h>

#define SOC_CONSTANT (4.00350877193)

#ifdef UART
  extern Uart & serial;
#else
  extern Serial_ & serial;
#endif

//soc_voltages maps a voltage to an soc %
//currently, this data is all for testing (real data needs to be recorded)
const double soc_voltages[100] = {7.420567, 7.972614, 8.203877, 8.333897, 8.422875, 8.490851, 8.548226, 8.593550, 8.633500, 8.671122,
8.702471, 8.731619, 8.756397, 8.780945, 8.810158, 8.842621, 8.858613, 8.875452, 8.890969, 8.902616,
8.916523, 8.928195, 8.940632, 8.950545, 8.961707, 8.972875, 8.983109, 8.990907, 8.999015, 9.010015,
9.020812, 9.031686, 9.041649, 9.051024, 9.060727, 9.068003, 9.075989, 9.083392, 9.091560, 9.099879,
9.107429, 9.113748, 9.120622, 9.127887, 9.135069, 9.142537, 9.149666, 9.156621, 9.162009, 9.168261,
9.173385, 9.179300, 9.185686, 9.190713, 9.195836, 9.201373, 9.205362, 9.209243, 9.213735, 9.237090,
9.238832, 9.242332, 9.245574, 9.249150, 9.253162, 9.256455, 9.260545, 9.265035, 9.271436, 9.275610,
9.282359, 9.286576, 9.321492, 9.328305, 9.337180, 9.348731, 9.357960, 9.371371, 9.384347, 9.400346,
9.417014, 9.433733, 9.448980, 9.467286, 9.613667, 9.635676, 9.660664, 9.686188, 9.716083, 9.746142,
9.780054, 9.814346, 9.853511, 9.894955, 9.939239, 9.986833, 10.040859, 10.098184, 10.169451, 10.298327
};

//Name: soc_mapping
//Purpose: Maps the voltage to the % of battery capacity left
//Inputs: voltage (the current voltage reading of the battery)
//Outputs: soc (the battery capacity left in %)
uint8_t soc_mapping(double voltage){
    
    uint8_t soc; 
    
    if (voltage >= soc_voltages[99]) {
        soc = 100;
        // #ifdef DEBUG
        //     format_terminal_for_new_entry();
        //     serial.printf("Voltage greater than %d\n", soc_voltages[99]);
        //     format_new_terminal_entry();
        // #endif
    }
    else if (voltage < soc_voltages[0]){ 
        soc = 0;
        // #ifdef DEBUG
        //     format_terminal_for_new_entry();
        //     serial.printf("Voltage less than %d\n", soc_voltages[0]);
        //     format_new_terminal_entry();
        // #endif
    }
    else {
        for (int percent = 0; percent < 100; percent++){
            if ( (voltage >= soc_voltages[percent]) && (voltage < soc_voltages[percent+1]) ){
                soc = percent + 1;
                // #ifdef DEBUG
                //     format_terminal_for_new_entry();
                //     serial.printf("Voltage greater than %d and less than %d\n", soc_voltages[percent], soc_voltages[percent + 1]);
                //     format_new_terminal_entry();
                // #endif
                break; 
            } 
        }
    }
    
    return soc;

}