/****************************************Copyright (c)****************************************************
**                                        
**                                 
**
**--------------File Info---------------------------------------------------------------------------------
** File name:			     main.c
** Last modified Date: 2016.3.27         
** Last Version:		   1.1
** Descriptions:		   ʹ�õ�SDK�汾-SDK_11.0.0
**				
**--------------------------------------------------------------------------------------------------------
** Created by:			FiYu
** Created date:		2016-7-1
** Version:			    1.0
** Descriptions:		�������ʵ��
**--------------------------------------------------------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "app_uart.h"
#include "app_error.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "boards.h"
#include "nrf_drv_timer.h"

/* ��������nRF51822�ܽ���Դ����
P0.00�����룺����������        P0.16�������OLED_CLK
P0.01��I2C_SCL��MPU6050			     P0.17�����룺�������S1
P0.02�������OLEDƬѡ					   P0.18�����룺�������S2
P0.03�����룺MPU6050�ж��ź�     P0.19�����룺�������S3
P0.04���ⲿADC���(��λ��)       P0.20�����룺�������S4
P0.05��I2C_SDA��MPU6050          P0.21�����������ָʾ��D1
P0.06��OLED��λ                  P0.22�����������ָʾ��D2
P0.07�������RGB��ɫָʾ������   P0.23�����������ָʾ��D3
P0.08�������OLED����/����ѡ��   P0.24�����������ָʾ��D4
P0.09��UART_TXD                  P0.25���洢��Ƭѡ
P0.10������������                P0.26��32.768KHz����
P0.11��UART_RXD                  P0.27��32.768KHz����
P0.12�����������������          P0.28���洢�����ݴ���
P0.13�������RGB��ɫָʾ������   P0.29���洢��ʱ��
P0.14�������RGB��ɫָʾ������   P0.30���洢�����ݴ���
P0.15�������OLED_MOSI
*/

#define	User_code		0xFD02		// �����������û���  


#define    IR_POWER      25   //����ܽţ���ô����Ϊ��ʡ��
#define    IR_RECE_PIN   14   //�������ܽ�
#define    Ir_Pin        nrf_gpio_pin_read(IR_RECE_PIN)// ��������������˿�

	        


/*************	�û�ϵͳ����	**************/
#define TIME		125		  	    // ѡ��ʱ��ʱ��125us, �������Ҫ����60us~250us֮��


/******************** �������ʱ��궨��, �û���Ҫ�����޸�	*******************/
//���ݸ�ʽ: Synchro,AddressH,AddressL,data,/data, (total 32 bit). 
#if ((TIME <= 250) && (TIME >= 60))			// TIME����������TIME̫������������ͣ�TIME̫С����������жϺ���ִ�С�
	#define	IR_sample			TIME		// �������ʱ�䣬��60us~250us֮�䣬
#endif


uint8_t	IR_SampleCnt;		          // ����������������ͨ�ö�ʱ���Ժ���ڼ������ۼӼ�¼
uint8_t	IR_BitCnt;			          // ��¼λ��
uint8_t	IR_UserH;			            // �û���(��ַ)���ֽ�
uint8_t	IR_UserL;			            // �û���(��ַ)���ֽ�
uint8_t	IR_data;			            // ��ԭ��
volatile uint8_t	IR_DataShit;		// ������
uint8_t	IR_code;			// �������	 	
bool		Ir_Pin_temp;		        // ��¼�������ŵ�ƽ����ʱ����
bool		IR_Sync;			        // ͬ����־��1�������յ�ͬ���źţ�0����û�յ���
bool		IrUserErr;		            // �û�������־
bool		IR_OK;			// ���һ֡�������ݽ��յı�־��0��δ�յ���1���յ�һ֡�������ݣ�

bool		F0;

#define IR_SYNC_MAX		(15000/IR_sample)	// ͬ���ź�SYNC ���ʱ�� 15ms(��׼ֵ9+4.5=13.5ms)
#define IR_SYNC_MIN		(9700 /IR_sample)	// ͬ���ź�SYNC ��Сʱ�� 9.5ms��(�����źű�׼ֵ9+2.25=11.25ms)
#define IR_SYNC_DIVIDE	(12375/IR_sample)	// ����13.5msͬ���ź���11.25ms�����źţ�11.25+��13.5-11.25��/2=12.375ms
#define IR_DATA_MAX		(3000 /IR_sample)	// �������ʱ��3ms    (��׼ֵ2.25 ms)
#define IR_DATA_MIN		(600  /IR_sample)	// ������Сʱ,0.6ms   (��׼ֵ1.12 ms)
#define IR_DATA_DIVIDE	(1687 /IR_sample)	// �������� 0��1,1.12+ (2.25-1.12)/2 =1.685ms
#define IR_BIT_NUMBER		32				// 32λ����




#define UART_TX_BUF_SIZE 256                         /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE 1                           /**< UART RX buffer size. */

const nrf_drv_timer_t TIMER_LED = NRF_DRV_TIMER_INSTANCE(0);

uint8_t Ir_Ver;

//****************************  �������ģ��  ********************************************
// �źŵ�1���½���ʱ��������������ü�������0��ʼ��������2���½���ʱ�̼������������ʱ��
// ��˼�����ÿ���źŴӵ͵�ƽ��ʼ���ߵ�ƽ�������ʱ�䣬Ҳ�����������ڡ�
void IR_RX(void)
{
	uint8_t	SampleTime;				// �ź�����
	IR_SampleCnt++;							// ��ʱ���Ժ���ڼ�����

	F0 = Ir_Pin_temp;						// ����ǰһ�δ˳���ɨ�赽�ĺ���˿ڵ�ƽ	  
	Ir_Pin_temp = Ir_Pin;					// ��ȡ��ǰ�����������˿ڵ�ƽ
	if(F0 && !Ir_Pin_temp)					// ǰһ�β����ߵ�ƽ���ҵ�ǰ�����͵�ƽ��˵���������½���
	{
		SampleTime = IR_SampleCnt;			// ��������
		IR_SampleCnt = 0;					// �������½��������������
		//******************* ����ͬ���ź� **********************************
		if(SampleTime > IR_SYNC_MAX)		IR_Sync = 0;	// �������ͬ��ʱ��, ������Ϣ��
		else if(SampleTime >= IR_SYNC_MIN)		// SYNC
		{
			if(SampleTime >= IR_SYNC_DIVIDE)    // ����13.5msͬ���ź���11.25ms�����ź� 
			{
				IR_Sync = 1;				    // �յ�ͬ���ź� SYNC
				IR_BitCnt = IR_BIT_NUMBER;  	// ��ֵ32��32λ�����źţ�
			}
		}
		//********************************************************************
		else if(IR_Sync)						// ���յ�ͬ���ź� SYNC
		{
			if((SampleTime < IR_DATA_MIN)|(SampleTime > IR_DATA_MAX)) IR_Sync=0;	// �������ڹ��̻����������
			else
			{
				IR_DataShit >>= 1;					// ����������1λ�����Ͷ��ǵ�λ��ǰ����λ�ں�ĸ�ʽ��
				if(SampleTime >= IR_DATA_DIVIDE)	IR_DataShit |= 0x80;	// ���������� 0����1

				//***********************  32λ���ݽ������ ****************************************
				if(--IR_BitCnt == 0)				 
				{
					IR_Sync = 0;					// ���ͬ���źű�־
					Ir_Ver = ~IR_DataShit;
					
					if((Ir_Ver) == IR_data)		// �ж�����������
					{
						if((IR_UserH == (User_code / 256)) && IR_UserL == (User_code % 256))
						{
								IrUserErr = 0;	    // �û�����ȷ
						}
						else	IrUserErr = 1;	    // �û������
							
						IR_code      = IR_data;		// ����ֵ
						IR_OK   = 1;			    // ������Ч
					}
				}
				// ��ʽ��  �û���L ���� �û���H ���� ���� ���� ������
				// ���ܣ�  �� ���û���L ���� �û���H ���� ���롱ͨ��3�ν��ս���������Ӧ�ֽڣ�
				//         ����д������Խ�ʡ�ڴ�RAMռ�ã����ǲ��������鱣�����⡣        
				//         ������ǰ�沿�ִ����ѱ������	:IR_DataShit |= 0x80;	          
				//**************************** �����յ�************************************************
				else if((IR_BitCnt & 7)== 0)		// 1���ֽڽ������
				{
					IR_UserL = IR_UserH;			// �����û�����ֽ�
					IR_UserH = IR_data;				// �����û�����ֽ�
					IR_data  = IR_DataShit;			// ���浱ǰ�����ֽ�
				}
			}
		}  
	}
}


void uart_error_handle(app_uart_evt_t * p_event)
{
    if (p_event->evt_type == APP_UART_COMMUNICATION_ERROR)
    {
        APP_ERROR_HANDLER(p_event->data.error_communication);
    }
    else if (p_event->evt_type == APP_UART_FIFO_ERROR)
    {
        APP_ERROR_HANDLER(p_event->data.error_code);
    }
}
/**
 * @brief Handler for timer events.
 */
void timer_led_event_handler(nrf_timer_event_t event_type, void* p_context)
{
    //static uint32_t i;
    //uint32_t led_to_invert = (1 << leds_list[(i++) % LEDS_NUMBER]);
    
    switch(event_type)
    {
        case NRF_TIMER_EVENT_COMPARE0:
            //nrf_gpio_pin_toggle(LED_1);
				    IR_RX();
            break;
        
        default:
            break;
    }    
}

/********************* ʮ������תASCII���� *************************/
unsigned char	HEX2ASCII(unsigned char dat)
{
	dat &= 0x0f;
	if(dat <= 9)	return (dat + '0');	//����0~9
	return (dat - 10 + 'A');			    //��ĸA~F
}
/**********************************************************************************************
 * ��  �� : main����
 * ��  �� : ��
 * ����ֵ : ��
 ***********************************************************************************************/ 
int main(void)
{
    uint32_t time_ms = 125; //Time(in miliseconds) between consecutive compare events.
    uint32_t time_ticks;
	
	  nrf_gpio_cfg_input(IR_RECE_PIN,NRF_GPIO_PIN_PULLUP);
	
	  LEDS_CONFIGURE(LEDS_MASK);
    LEDS_OFF(LEDS_MASK);
	
	  nrf_gpio_cfg_output(IR_POWER);
	
    uint32_t err_code;
    const app_uart_comm_params_t comm_params =
    {
          RX_PIN_NUMBER,
          TX_PIN_NUMBER,
          RTS_PIN_NUMBER,
          CTS_PIN_NUMBER,
          APP_UART_FLOW_CONTROL_DISABLED,
          false,
          UART_BAUDRATE_BAUDRATE_Baud115200
    };

    APP_UART_FIFO_INIT(&comm_params,
                         UART_RX_BUF_SIZE,
                         UART_TX_BUF_SIZE,
                         uart_error_handle,
                         APP_IRQ_PRIORITY_LOW,
                         err_code);

    APP_ERROR_CHECK(err_code);
		
		//Configure TIMER_LED for generating simple light effect - leds on board will invert his state one after the other.
    err_code = nrf_drv_timer_init(&TIMER_LED, NULL, timer_led_event_handler);
    APP_ERROR_CHECK(err_code);
    
    time_ticks = nrf_drv_timer_us_to_ticks(&TIMER_LED, time_ms);
    
    nrf_drv_timer_extended_compare(
         &TIMER_LED, NRF_TIMER_CC_CHANNEL0, time_ticks, NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK, true);
    
    nrf_drv_timer_enable(&TIMER_LED);
    nrf_gpio_pin_set(IR_POWER);
    while (true)
    {
        if(IR_OK)	 	                    // ���յ�һ֡�����ĺ�������
				{
					  while(app_uart_put(HEX2ASCII(IR_code >> 4)) != NRF_SUCCESS); 
					  while(app_uart_put(HEX2ASCII(IR_code)) != NRF_SUCCESS); 
					  IR_OK = 0;		              // ���IR�����±�־
				}
    }
}
/********************************************END FILE*******************************************/
