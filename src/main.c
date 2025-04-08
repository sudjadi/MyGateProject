/*
Auto gate:
button1 : to open/close both gate
button2 : to open/close both gate via tuya switch
button3 : connected to reid switch right
button4 : connected to reid swtich left
add comment
*/
#include "include/uartservice.h"
#include "include/adc.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/timer/nrf_rtc_timer.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
int adc_value[2];                       //buffer for ADC
#define PWM_LED0_NODE DT_NODELABEL(pwm_led0)
#define PWM_LED1_NODE DT_NODELABEL(pwm_led1)
// Get the PWM device and channel information
static const struct pwm_dt_spec pwm_led0 = PWM_DT_SPEC_GET(PWM_LED0_NODE);
static const struct pwm_dt_spec pwm_led1 = PWM_DT_SPEC_GET(PWM_LED1_NODE);
// PWM DEFINITION
#define PERIOD                  1000     //ms
#define DUTYCYCLE               750      //10ms high

#define GATETIMERLIMIT          250     // 250 x 100ms timer = 25 seconds
#define SLEEPTIMER              300     // 300 x 100ms
/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)                        //for right actuator
#define LED1_NODE DT_ALIAS(led1)                        //left actuator
#define LED2_NODE DT_ALIAS(led2)                        //PWM Direction
#define LED3_NODE DT_ALIAS(led3)                        
#define SW1_NODE DT_ALIAS(sw0)                          //gate button 
#define SW2_NODE DT_ALIAS(sw1)                          //Tuya switch : look for momentary push signal
#define SW3_NODE DT_ALIAS(sw2)                          //reid switch right
#define SW4_NODE DT_ALIAS(sw3)                          //reid switch left

int btn0_count, btn1_count,btn2_count,btn3_count,sleepCount,ledFlashTimer;
/// for pwm /////////////////////////
uint8_t state = 0;
uint32_t pulse_width_ms0 = 0;  // Initial pulse width (off)
uint32_t pulse_width_ms1 = 20000;	//PWM_MSEC(20);
uint32_t step = 1;            //change this value to control the brightness step
uint32_t gateTimer = 0;
//flag..
bool gateIsClosing = false;
bool gateIsOpening = false;
bool gateIsClose = false;
bool pwm_LHisRunning = false;
bool pwm_RHisRunning = false;
bool timerisRunning = false;
// button0 
bool btn0_validPush = false;    //valid push is press (100-1900ms) and release
bool btn0_validHold = false;    //valid hold is press 2000ms
// button1 : tuya  switch
bool btn1_validPush = false; //valid contact is 100ms
// button2/3 : reid switch 
bool btn2_validContact = false;
bool btn3_validContact = false;

bool timeToReadADC = false;

//state machine
#define IDLESTATE 0
#define RUNNINGSTATE 1
#define MANUALSTATE 2
#define WAITSTATE 3
#define ERRORSTATE 4

/*
 * A build error on this line _means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led_0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);  //gate actuator RH
static const struct gpio_dt_spec led_1 = GPIO_DT_SPEC_GET(LED1_NODE, gpios);  //gate actuator LH
static const struct gpio_dt_spec led_2 = GPIO_DT_SPEC_GET(LED2_NODE, gpios);
static const struct gpio_dt_spec led_3 = GPIO_DT_SPEC_GET(LED3_NODE, gpios);
static const struct gpio_dt_spec btn_0 = GPIO_DT_SPEC_GET(SW1_NODE, gpios);   //gate switch
static const struct gpio_dt_spec btn_1 = GPIO_DT_SPEC_GET(SW2_NODE, gpios);   //tuyaswitch
static const struct gpio_dt_spec btn_2 = GPIO_DT_SPEC_GET(SW3_NODE, gpios);   //Reid close sensor RH
static const struct gpio_dt_spec btn_3 = GPIO_DT_SPEC_GET(SW4_NODE, gpios);   //Reid close sensor LH

struct k_timer timer1,timer2;

// 10ms timer interrupt handler
void timer1_callback(struct k_timer *timer_id)
{
 //   printk("Timer1 expired!\n");
        /// debounce push button
        sleepCount++;
        // button0:
        // - press and release (100ms - 1500ms) : open/close/stop gate 
        // - long press (>1500ms) : resume open/close.
        // to open/close gate must push and release button
        // hold button for 2 seconds wil activate open/close until release
        if (gpio_pin_get_raw(btn_0.port,btn_0.pin)==0){
                sleepCount=0;
                if (btn0_count<200) btn0_count++;                
                if (btn0_count==200) {
                    btn0_validHold=true;
                //    printk("Button0 Is held /r/n ");
                }
        } else {
                if ((btn0_count>10) & (btn0_count<150)) btn0_validPush=true;
                  else
                  if (btn0_validHold==true) btn0_validHold=false;    
                btn0_count=0;
        //        printk("Button0 Is Released");
        }
        // debunce for tuya gate...
        // button1:
        // - switch is closed : min 100ms contact, once register button must be release to register another conctact
        if (gpio_pin_get_raw(btn_1.port,btn_1.pin)==0){
                sleepCount=0;
                if (btn1_count<1000) btn1_count++;
        } else {
                if (btn1_count>10) btn1_validPush=true;
                btn1_count=0;
        }
       // debunce for tuya gate...
       // - switch is closed : min 100ms contact, once register button must be release to register another conctact
        if (gpio_pin_get_raw(btn_2.port,btn_2.pin)==0){
                sleepCount=0;
                if (btn2_count<5) {
                        btn2_count++;   
                        if (btn2_count==5) {
                                btn2_validContact=true;    
                                printk("Reid RH switch valid contact /r/n");
                        }
                }
        } else {
                if (btn2_validContact) {
                        btn2_validContact = false;
                        printk("Reid RH switch release /r/n");
                }
                btn2_count=0;
        }
        // - switch is closed : min 100ms contact, once register button must be release to register another conctact
        if (gpio_pin_get_raw(btn_3.port,btn_3.pin)==0){
                sleepCount=0;
                if (btn3_count<5) {
                        btn3_count++;   
                        if (btn3_count==5) {
                                btn3_validContact=true;    
                                printk("Reid LH switch valid contact /r/n");
                        }
                }
        } else {
                if (btn3_validContact){
                        btn3_validContact=false;
                        printk("Reid LH switch release /r/n");
                }
                btn3_count=0;   
        }        

}
  
 
// called when gate is moving
void timer2_callback(struct k_timer *timer_id){
        ledFlashTimer++; 
        if (ledFlashTimer>4) {
                gpio_pin_toggle_dt(&led_3);
                ledFlashTimer=0;
        }
        
        timeToReadADC=true;
        gateTimer--;
//	gpio_pin_toggle_dt(&led_3);
//	pulse_width_ms1+=1000;
//	if(pulse_width_ms1>20000) pulse_width_ms1=20000;
//	pwm_set_pulse_dt(&pwm_led1, PWM_USEC(pulse_width_ms1));
}

K_TIMER_DEFINE(timer1, timer1_callback, NULL);
K_TIMER_DEFINE(timer2, timer2_callback, NULL);

// Interrupt Handler Function
// Interrupt Handler Function
// any button_pressed will start timer1 
void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
            printk("Button Pressed!");
                btn0_count=0;
        	gpio_pin_interrupt_configure_dt(&btn_0,GPIO_INT_DISABLE);
                gpio_pin_interrupt_configure_dt(&btn_1,GPIO_INT_DISABLE);
                gpio_pin_interrupt_configure_dt(&btn_2,GPIO_INT_DISABLE);
                gpio_pin_interrupt_configure_dt(&btn_3,GPIO_INT_DISABLE);
                //if(!timerisRunning){
                //   timerisRunning=true;
                   k_timer_start(&timer1, K_MSEC(10), K_MSEC(10)); // 10-msecond timer
                   sleepCount=0;     //sleep timer = 30 seconds
                //}
        
        }

void checkGateStatus(){
        /*see current gate status
        1. if RH and LH are close : set direction as 1 
        2. if RH and LH are open  : set direction as 0
        3. if RH and LH are not in the same position : set directeion as 0
        */
       if (btn2_validContact&btn3_validContact) gateIsClose=true; else gateIsClose=false;

}
void openGate(){
        printk("Opening Gate %d \n\a",state);
        gateIsOpening=true;
        gateIsClosing=false;
        pwm_set_dt(&pwm_led0,PERIOD,DUTYCYCLE);
        pwm_LHisRunning=true;
        pwm_set_dt(&pwm_led1,PERIOD,DUTYCYCLE);
        pwm_RHisRunning=true;        
        gpio_pin_set_dt(&led_3,0);      //pwm direction
        timeToReadADC=false;
        gateTimer=GATETIMERLIMIT;
        ledFlashTimer=0;
        k_timer_start(&timer2, K_MSEC(100), K_MSEC(100)); // 100 msecond timer

}
void closeGate(){
        
        gateIsOpening=false;
        if(!btn2_validContact){
                pwm_set_dt(&pwm_led0,PERIOD,DUTYCYCLE);
                pwm_LHisRunning=true;
        }
        if(!btn3_validContact){
                pwm_set_dt(&pwm_led1,PERIOD,DUTYCYCLE);
                pwm_RHisRunning=true;
        }
        if (pwm_LHisRunning | pwm_RHisRunning) {
                printk("Closing Gate %d \n\a",state);
                gpio_pin_set_dt(&led_3,1);
                timeToReadADC=false;
                gateTimer=GATETIMERLIMIT;
                ledFlashTimer=0;
                k_timer_start(&timer2, K_MSEC(100), K_MSEC(100)); // 100 msecond timer
                gateIsClosing=true;
        }
}
void stopGate(){
        printk("Gate Stopped %d\n\a",state);
        pwm_set_dt(&pwm_led0,PERIOD,0);
        pwm_set_dt(&pwm_led1,PERIOD,0);
        gpio_pin_set_dt(&led_0,1);
        gpio_pin_set_dt(&led_1,1);
        pwm_LHisRunning=false;
        pwm_RHisRunning=false;
        k_timer_stop(&timer2);                  //stop timer2 (for reading adc)        
}


int stateMachine(){
        switch(state) {
                case IDLESTATE:
                        // check reid sensor to see
                        // wait for button pressed
                        checkGateStatus();
                        if (btn0_validPush||btn1_validPush) {
                                // toggle gate
                                btn0_validPush=false;
                                btn1_validPush=false;
                                state=RUNNINGSTATE;
                                // decide whether to open or close gate
                                if(gateIsClose) {
                                        // gate is close, open the gate
                                        openGate();
                                        break;
                                } else {
                                        // get could be open or partially open
                                        // just reverse previous direction
                                        if (gateIsClosing) openGate();
                                        else {
                                               closeGate();
                                               if (!gateIsClosing) state=IDLESTATE;   //gate already closed
                                        }
                                        break;

                                }

                        } else
                        if (btn0_validHold) {
                                state=MANUALSTATE;
                                if (gateIsClosing) closeGate(); //continue previous movement
                                else if (gateIsOpening) openGate();  //continue previous movement
                                else {  //no movement since powerup
                                        if(gateIsClose) {
                                                openGate();
                                        } else {;
                                                closeGate();
                                        }

                                }

                         }
                        break;
                case RUNNINGSTATE:
                        // gate is opening / closing 
                        // if button is pressed, stop gate
                        // otherwise check current draw, if current draw exceeds threshold stop the gate
                        // wait for button pressed:
                        if (btn0_validHold) {
                                stopGate();
                                state=WAITSTATE;
                                break;
                        } else
                        if (btn0_validPush||btn1_validPush) {
                                btn0_validPush=false;
                                btn1_validPush=false;
                                stopGate();
                                state=0;
                                break;
                        }
                        // read current draw and check for overcurrent
                        if (timeToReadADC) {
                                readADC(&adc_value);
                                timeToReadADC=false;
                                if(!pwm_LHisRunning) adc_value[0]=0;
                                if(!pwm_RHisRunning) adc_value[1]=0;
                                printk("ADC Value: %d and %d\n", adc_value[0],adc_value[1]);
                                bt_nus_send(NULL, adc_value, 2);
                        }
                        // when closing, check if reid switch is detected close
                        if (gateIsClosing){
                                if (btn2_validContact) {
                                        pwm_set_dt(&pwm_led0,PERIOD,0);
                                        gpio_pin_set_dt(&led_0,0);
                                        pwm_LHisRunning=false;
                                        if (!pwm_RHisRunning) {
                                                state=0;
                                                k_timer_stop(&timer2);                  //stop timer2 (for reading adc) 
                                                break;
                                        }
                                }
                                if (btn3_validContact) {
                                        pwm_set_dt(&pwm_led1,PERIOD,0);
                                        gpio_pin_set_dt(&led_1,0);
                                        pwm_RHisRunning=false;
                                        if (!pwm_LHisRunning) {
                                                state=0;
                                                k_timer_stop(&timer2);                  //stop timer2 (for reading adc) 
                                                break;
                                        }
                                }
                        }
                        // check if time limit to open/close gate reached
                        if (gateTimer==0) {
                                stopGate();
                                if(btn0_validHold) state=WAITSTATE;
                                else state=0;
                        }

                        break;
                case MANUALSTATE:
                        // gate is manually opening / closing 
                        // if button is pressed, stop gate
                        // otherwise check current draw, if current draw exceeds threshold stop the gate
                        // wait for butto pressed:
                        if (!btn0_validHold) {
                                stopGate();
                                state=0;
                                break;
                        }
                        if (timeToReadADC) {
                                readADC(&adc_value);
                                timeToReadADC=false;
                                if(!pwm_LHisRunning) adc_value[0]=0;
                                if(!pwm_RHisRunning) adc_value[1]=0;
                                printk("ADC Value: %d and %d\n", adc_value[0],adc_value[1]);
                                bt_nus_send(NULL, adc_value, 2);

                        }
                        // when closing, check if reid switch is detected close
                        if (gateIsClosing){
                                if (btn2_validContact) {
                                        pwm_set_dt(&pwm_led0,PERIOD,0);
                                        gpio_pin_set_dt(&led_0,0);
                                        pwm_LHisRunning=false;
                                        if (!pwm_RHisRunning) {
                                                state=WAITSTATE;
                                                k_timer_stop(&timer2);                  //stop timer2 (for reading adc) 
                                                break;
                                        }
                                }
                                if (btn3_validContact) {
                                        pwm_set_dt(&pwm_led1,PERIOD,0);
                                        gpio_pin_set_dt(&led_1,0);
                                        pwm_RHisRunning=false;
                                        if (!pwm_LHisRunning) {
                                                state=WAITSTATE;
                                                k_timer_stop(&timer2);                  //stop timer2 (for reading adc) 
                                                break;
                                        }
                                }
                        }
                                                // check if time limit to open/close gate reached
                        if (gateTimer==0) {
                                stopGate();
                                state=WAITSTATE;
                        }
                        break;
                case WAITSTATE:
                        if (!btn0_validHold) state=0;
                        break;
                        // gate has stopped must wait


        }
}
        
int main(void)
{       int err;
        err = initUsartService();
        if (err) return 0;
        err= initADC();
        if (err) return 0;
    
        // pwm ///////////////////////////////
        if (!device_is_ready(pwm_led0.dev)) {
		printk("Error: PWM device %s is not ready\n", pwm_led0.dev->name);
		return 1;
	}
	if (!device_is_ready(pwm_led1.dev)) {
		printk("Error: PWM device %s is not ready\n", pwm_led1.dev->name);
		return 1;
	}

	if (!gpio_is_ready_dt(&led_0)|!gpio_is_ready_dt(&led_1) | !gpio_is_ready_dt(&led_2)|!gpio_is_ready_dt(&led_3)) return 0;
		
	if (!gpio_is_ready_dt(&btn_0)|!gpio_is_ready_dt(&btn_1)) return 0;
	
        err = gpio_pin_configure_dt(&led_0, GPIO_OUTPUT_ACTIVE);
	if (err) return 0;
	err = gpio_pin_configure_dt(&led_1, GPIO_OUTPUT_ACTIVE);
	if (err) return 0;
	err = gpio_pin_configure_dt(&led_2, GPIO_OUTPUT_ACTIVE);
	if (err) return 0;
	err = gpio_pin_configure_dt(&led_3, GPIO_OUTPUT_ACTIVE);
	if (err) return 0;
	err = gpio_pin_configure_dt(&btn_0,GPIO_INPUT |GPIO_PULL_UP);
	if (err) return 0;
	err = gpio_pin_configure_dt(&btn_1,GPIO_INPUT |GPIO_PULL_UP);
	if (err) return 0;
        err = gpio_pin_configure_dt(&btn_2,GPIO_INPUT |GPIO_PULL_UP);
	if (err) return 0;
        err = gpio_pin_configure_dt(&btn_3,GPIO_INPUT |GPIO_PULL_UP);
	if (err) return 0;
        // setup interrupt for button0
	err = gpio_pin_interrupt_configure_dt(&btn_0,GPIO_INT_EDGE_FALLING);
	if (err) return 0;
	err = gpio_pin_interrupt_configure_dt(&btn_1,GPIO_INT_EDGE_FALLING);
	if (err) return 0;
        err = gpio_pin_interrupt_configure_dt(&btn_2,GPIO_INT_EDGE_FALLING);
	if (err) return 0;
	err = gpio_pin_interrupt_configure_dt(&btn_3,GPIO_INT_EDGE_FALLING);
	if (err) return 0;
        gpio_pin_set_dt(&led_0,0);
        gpio_pin_set_dt(&led_1,0);
        gpio_pin_set_dt(&led_2,0);
        gpio_pin_set_dt(&led_3,0);
        // Initialize and add callback
/*
        static struct gpio_callback btn_0_cb,btn_1_cb;
	gpio_init_callback(&btn_0_cb, button0_pressed, BIT(btn_0.pin));
	gpio_add_callback(btn_0.port, &btn_0_cb);
	gpio_init_callback(&btn_1_cb, button1_pressed, BIT(btn_1.pin));
	gpio_add_callback(btn_1.port, &btn_1_cb);
*/
        static struct gpio_callback btn_cb;
        //interupt on any button press
        gpio_init_callback(&btn_cb,button_pressed,BIT(btn_0.pin)|BIT(btn_1.pin)|BIT(btn_2.pin)|BIT(btn_3.pin));
        gpio_add_callback(btn_0.port,&btn_cb);
        printk("All innitialised and ready...");
        for (;;) {
                //	dk_set_led(RUN_STATUS_LED, (++blink_status) % 2);
                if (sleepCount>3000) {
                        printk("Deep Sleep \r\n");
                        k_timer_stop(&timer1);                  //stop timer2 (for reading adc)      
                        k_timer_stop(&timer2);
                        stopGate();
                 //       gpio_pin_interrupt_configure_dt(&btn_0,GPIO_INT_ENABLE);
                       // setup interrupt for button0
	                err = gpio_pin_interrupt_configure_dt(&btn_0,GPIO_INT_EDGE_FALLING);
	                if (err) return 0;
	                err = gpio_pin_interrupt_configure_dt(&btn_1,GPIO_INT_EDGE_FALLING);
	                if (err) return 0;
                        err = gpio_pin_interrupt_configure_dt(&btn_2,GPIO_INT_EDGE_FALLING);
	                if (err) return 0;
	                err = gpio_pin_interrupt_configure_dt(&btn_3,GPIO_INT_EDGE_FALLING);
                        k_cpu_idle();  // Sleep until an interrupt occurs       k_sleep(K_MSEC(1000));
                        sleepCount=0;
                        if (state!=MANUALSTATE) state=0;
                        printk("wake up \r\n");

                }
                stateMachine();
               
                }
}

          