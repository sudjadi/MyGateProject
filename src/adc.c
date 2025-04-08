#include <zephyr/drivers/adc.h>

#define ADC_DEVICE_NAME DT_LABEL(DT_NODELABEL(adc))  // Get ADC device
#define ADC_CHANNEL1 0   // Select the ADC channel AIN0
#define ADC_CHANNEL2 1   // Select the ADC channel AIN1

#define ADC_RESOLUTION 8  // Set 12-bit resolution
#define ADC_GAIN ADC_GAIN_4  // Set gain (adjust as needed)
#define ADC_REFERENCE ADC_REF_INTERNAL  // Internal reference voltage
#define ADC_ACQUISITION_TIME ADC_ACQ_TIME(ADC_ACQ_TIME_MICROSECONDS, 10) // 10us

static const struct device *adc_dev = DEVICE_DT_GET(DT_NODELABEL(adc));
static int16_t adc_buffer[2];  // Buffer to store ADC value

static struct adc_channel_cfg adc_cfg1 = {
    .gain = ADC_GAIN,
    .reference = ADC_REFERENCE,
    .acquisition_time = ADC_ACQUISITION_TIME,
    .channel_id = ADC_CHANNEL1,
    .differential = 0,
    .input_positive = SAADC_CH_PSELP_PSELP_AnalogInput0, // Maps to P0.0
};
static struct adc_channel_cfg adc_cfg2 = {
    .gain = ADC_GAIN,
    .reference = ADC_REFERENCE,
    .acquisition_time = ADC_ACQUISITION_TIME,
    .channel_id = ADC_CHANNEL2,
    .differential= 0,
    .input_positive = SAADC_CH_PSELP_PSELP_AnalogInput1, // Maps to P0.02
};
static struct adc_sequence adc_seq = {
    .channels = BIT(ADC_CHANNEL1)|BIT(ADC_CHANNEL2),
    .buffer = &adc_buffer,
    .buffer_size = sizeof(adc_buffer),
    .resolution = ADC_RESOLUTION,
};



int initADC(void) {
    int err;

    if (!device_is_ready(adc_dev)) {
        printk("ADC device not ready\n");
        return 1;
    }

    err = adc_channel_setup(adc_dev, &adc_cfg1);
    if (err) {
        printk("ADC channel setup failed (err %d)\n", err);
        return 0;
    }
    err = adc_channel_setup(adc_dev, &adc_cfg2);
    if (err) {
        printk("ADC channel setup failed (err %d)\n", err);
        return 1;
    }
    
    
    printk("ADC initialized, reading values...\n");
    return 0;
}

int readADC(int *buffer){
        int err;
   
        err = adc_read(adc_dev, &adc_seq);
        if (err) {
            printk("ADC read failed (err %d)\n", err);
            return err;
        } else {
            buffer[0]=adc_buffer[0];
            buffer[1]=adc_buffer[1];
 //           printk("ADC Value: %d and %d\n", adc_buffer[0],adc_buffer[1]);
            return 0;
        }
}

    
