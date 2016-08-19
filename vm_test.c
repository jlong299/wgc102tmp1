/*********************************************************************************/
/* (C) 2001-2012 Altera Corporation. All rights reserved.                        */
/* Your use of Altera Corporation's design tools, logic functions and other      */
/* software and tools, and its AMPP partner logic functions, and any output      */
/* files any of the foregoing (including device programming or simulation        */
/* files), and any associated documentation or information are expressly subject */
/* to the terms and conditions of the Altera Program License Subscription        */
/* Agreement, Altera MegaCore Function License Agreement, or other applicable    */
/* license agreement, including, without limitation, that your use is for the    */
/* sole purpose of programming logic devices manufactured by Altera and sold by  */
/* Altera or its authorized distributors.  Please refer to the applicable        */
/* agreement for further details.                                                */
/*********************************************************************************/

/*********************************************************************************/
/* INCLUDE FILES                                                                 */
/*********************************************************************************/

#include "vm_test.h"
#include <string.h>
#include <stdlib.h>

#include "altera_message.h"
#include "altera_hwif.h"
#include "vm_test_reg.h"

#include <pthread.h>
#include <stdio.h>

#include <unistd.h>


#include <time.h>
#include <math.h>

#include "alt_hrd_turbo.h"
#include "time_meas.h"
#include "ring_buffer.h" 

/********************************************************************************/
/* Global Variables  & Memories                                                 */
/********************************************************************************/

altera_message_handler_s        test_message_id;

altr_uint32                     HEADER_packet_sample_words[8][MAXIMUM_PKT_SIZE_WORDS];
altr_uint8                      HEADER_packet_sample_bytes[VFAP_FRAMEWORK_MAX_PACKET_LENGTH_BYTES];

altr_uint8                      TX_buffer_bytes[256][VFAP_FRAMEWORK_MAX_PACKET_LENGTH_BYTES];
altr_uint8                      RX_buffer_bytes     [VFAP_FRAMEWORK_MAX_PACKET_LENGTH_BYTES];

altr_uint32                     length_src;
altr_avalon_conn_s conn_g;


#ifdef  ALTR_OS_LINUX
#include <pthread.h>
pthread_t      sending_thread;
pthread_t      receiving_thread;
pthread_attr_t pth_attr;

int            stop_sending_thread_flag   = 0;
int            stop_receiving_thread_flag = 0;

int            start_sending_thread_flag   = 0;
int            start_receiving_thread_flag = 0;

struct         itimerval timer;

#endif

altr_uint32                bytes_counter;
altr_uint32                bytes_counter_delta;
altr_uint32                enable_timer_irq;
altr_uint32                counters_mode;
altr_uint32                mac_relevant_stats;

altr_uint32                hw_acc_stats[9];
altr_uint32                hw_acc_stats_delta[9];



int opp_enabled;

FILE* open_file_for_read(const char* input_file)
{
    FILE* f = fopen(input_file, "r");
    if (f == NULL) {
        printf("\n\nError when opening file for read: %s\n", input_file);
        exit(1);
    } else {
        /* printf("\n\nOpening following file for reading : %s\n", input_file); */
    } 
    return f;
}

FILE* open_file_for_write(const char* output_file)
{
    FILE* f = fopen(output_file, "wt");
    if (f == NULL) {
        printf("\n\nError when opening file for write: %s\n", output_file);
        exit(1);
    } else {
        /* printf("\n\nOpening following file for writing : %s\n", output_file); */
    } 
 
    return f;
}




/* Find difference in time */
struct timespec diff_time(struct timespec start, struct timespec end)
{
    struct timespec temp;
    if ((end.tv_nsec - start.tv_nsec) < 0) {
        temp.tv_sec     = end.tv_sec - start.tv_sec - 1;
        temp.tv_nsec    = 1000000000 + end.tv_nsec - start.tv_nsec;
    } else {
        temp.tv_sec     = end.tv_sec - start.tv_sec;
        temp.tv_nsec    = end.tv_nsec - start.tv_nsec;
    }
    return temp;

} /* timespec diff_time(timespec start, timespec end) */


/********************************************************************************/
/* Main                                                                         */
/********************************************************************************/

int main(int argc, char* argv[]){

    int                               rc_i;
    char                              key;
                                      
    vfap_framework_ret_status         vfap_framework_rc;
    altr_uint32                       length_bytes;
                                      
    altr_uint32                       header_id;
    altr_uint32                       timeout;

    altr_uint32                       data_in, data_out;
    altr_uint32                       var_input;
                                      
    vfap_acc_operation_handler_s      vfap_acc_operation;

    opp_enabled=1;

	printf("\n Test: main start \n");

    if(altera_hwif_init(&conn_g)!= ALTERA_HWIF_RET_SUCCESS){
        block_for_char("Error in initlising hwif, press a key to continue");
        return 0;
    }

    //printf("calling hwif_connect_dut\n");

    if (altera_hwif_connect_dut(argc, argv) != ALTERA_HWIF_RET_SUCCESS){

        block_for_char("Error in connecting to hw, press a key to continue");
        return 0;
    }

    rc_i = test_init(); //No configuration file for the time being

    //block_for_char(0);

    if(rc_i == -1) {
        altera_message_print(&test_message_id, ALTERA_MESSAGE_ERROR, "[main] Error in test_init!");
        block_for_char("Type a letter to exit");
        return 0;
    }

//    flush_screen();

    while(1){

        printf(" turbo Test Menu v 1.11: \n");
        printf("__________________________\n\n");
        printf("\n");
        printf("A - Send Then Recieve a packet via DMA INT.\n");
        printf("B - Accelerate Data via DMA.\n");

        printf("\n\n");

        printf("C - Initialise TX RING.\n");
        printf("D - Initialise RX RING.\n");
        printf("\n");
        printf("E - Clear TX RING.\n");
        printf("F - Clear RX RING.\n");
        printf("\n");
        printf("G - START DMA.\n");
        printf("H - STOP DMA.\n");

  

        printf("\n");
        printf("@ - Turbo test.ring\n");



        printf("\n\n");
        printf("I - Start Sending Thread.\n");
        printf("J - Stop Sending Thread.\n");
/*
        printf("\n");

        printf("K - Start Receiving Thread.\n");
        printf("L - Stop Receiving Thread.\n");
*/
        printf("\n");

        printf("M - Monitor and Performance Statistics.\n");

        printf("\n\n");
        //printf("N - Init 40G MAC.\n");
        printf("O - Internal LoopBack (FOR MAC ONLY).\n");

        printf("\n\n");

        printf("\nVM Debug Menu: \n");
        printf("__________________________\n\n");
        printf("\n");
        printf("R - Init DMA.\n");
        printf("S - DMA Version.\n");
        printf("T - DMA Test.\n");
        printf("U - Access MSI-X Table.\n");
        printf("V - Access MSI-X PBA.\n");
        printf("W - Run Debug Code.\n");
        printf("\n\n");
        
        printf("\n\n");
        printf("Q - Quit.\n\n");

        printf(">> ");

        key = getchar();

        switch (tolower(key)){

		case '@':

			alt_turbo_example();
            block_for_char("turbo finished");
			break;

        case 'a':

            timeout = 10;

            print_header_list();
            parse_altr_uint32("What Header do you want to send (0-8) ", &header_id);

            printf("[main]: enter length in hexadecimal bytes (0x50 - 0x800) = ");
            scanf("%x", &length_bytes);

            if(header_id == 8) {

                vfap_framework_rc = vfap_dma_int_send_packet(&HEADER_packet_sample_bytes[0], length_bytes);

            } else {

                vfap_framework_rc = vfap_dma_int_send_packet((altr_uint8*) &HEADER_packet_sample_words[header_id][0], length_bytes);
            }

            if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {
                altera_message_print(&test_message_id, ALTERA_MESSAGE_ERROR, "[main] Error (%d) from vfap_dma_int_send_packet!", vfap_framework_rc);
            }


            altera_message_print(&test_message_id, ALTERA_MESSAGE_ALL, "Sending Packet Completed");

            while (timeout) 
            {
                vfap_framework_rc = vfap_dma_int_recieve_packet(&RX_buffer_bytes[0], &length_bytes);

                if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {
                    //altera_message_print(&test_message_id, ALTERA_MESSAGE_ERROR, "[main] Error (%d) from vfap_dma_int_recieve_packet!", vfap_framework_rc);
                    //altera_message_print(&test_message_id, ALTERA_MESSAGE_ALL,   "[main] No Packets Available!");
                    vm_sleep(100);
                }
                else {
                    pirnt_rx_pkt_bytes(length_bytes);
                    altera_message_print(&test_message_id, ALTERA_MESSAGE_ALL, "Receiving Packet Sucessfull, timeout = %d", timeout);
                    break;
                }

                timeout --;
            }

            if(timeout == 0) {
                altera_message_print(&test_message_id, ALTERA_MESSAGE_ALL, "[main] Time out!");
                altera_message_print(&test_message_id, ALTERA_MESSAGE_ALL, "[main] Error (%d) from vfap_dma_int_recieve_packet!", vfap_framework_rc);
                altera_message_print(&test_message_id, ALTERA_MESSAGE_ALL, "[main] No Packets Available!");
            }

            block_for_char("Sending/Receiving Packet operation finished");
            break;

        case 'b':

            memset(&vfap_acc_operation, 0, sizeof(vfap_acc_operation_handler_s));

            print_header_list();
            parse_altr_uint32("What Header do you want to send (0-8) ", &header_id);

            printf("[main]: enter source length in hexadecimal bytes (0x50 - 0x2000) = ");
            scanf("%x", &length_bytes);

            vfap_acc_operation.data.source_data.address        = (header_id == 8) ? &HEADER_packet_sample_bytes[0] : (altr_uint8*) &HEADER_packet_sample_words[header_id][0];
            vfap_acc_operation.data.accelerated_data.address   = &RX_buffer_bytes[0];
            vfap_acc_operation.data.source_data.len_bytes      = length_bytes;


            printf("[main]: enter destination length in hexadecimal bytes (0x50 - 0x2000) = ");
            scanf("%x", &length_bytes);
            vfap_acc_operation.data.accelerated_data.len_bytes = length_bytes;

            vfap_acc_operation.param.acc_arg0 = 0xbabecafe;
            vfap_acc_operation.param.acc_arg1 = 0xdeadbabe;
            vfap_acc_operation.param.interrupt_ack_conf = VFAP_DMA_TRANS_CONF_INTERRUPT_WHEN_RECEIVED;

            vfap_framework_rc = vfap_accelerate_my_data(&vfap_acc_operation);

            if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {
                altera_message_print(&test_message_id, ALTERA_MESSAGE_ERROR, "[main] Error (%d) from vfap_accelerate_my_data!", vfap_framework_rc);
                block_for_char(0);
                break;
            }
            else {
                pirnt_rx_pkt_bytes(length_bytes);
                block_for_char("Data Accelerated");
            }
            break;

        case 'c':

            printf("[main]: enter packet length in hexadecimal bytes (0x50 - 0x2000) = ");
            scanf("%x", &length_src);

            if(initialise_tx_ring() != 0) {
                block_for_char("initialise_tx_ring failed!");
            }
            block_for_char("initialise_tx_ring Completed Sucessfully!");

            break;

        case 'd':

            printf("[main]: enter packet length in hexadecimal bytes (0x50 - 0x2000) = ");
            scanf("%x", &length_src);

            if(initialise_rx_ring() != 0) {
                block_for_char("initialise_rx_ring failed!");
            }
            block_for_char("initialise_rx_ring Completed Sucessfully!");

            break;

        case 'e':

            if(clear_tx_ring() != 0) {
                block_for_char("clear_tx_ring failed!");
            }
            block_for_char("clear_tx_ring Completed Sucessfully!");

            break;

        case 'f':

            if(clear_rx_ring() != 0) {
                block_for_char("clear_rx_ring failed!");
            }
            block_for_char("clear_rx_ring Completed Sucessfully!");

            break;

        case 'g':

            if(start_dma() != 0) {
                block_for_char("start_dma failed!");
            }
            block_for_char("start_dma Completed Sucessfully!");

            break;

        case 'h':

            if(stop_dma() != 0) {
                block_for_char("stop_dma failed!");
            }
            block_for_char("stop_dma Completed Sucessfully!");

            break;

        case 'i':

            printf("[main]: enter packet length in hexadecimal bytes (0x50 - 0x800) = ");
            scanf("%x", &length_src);

            if(start_sending_thread() == 0) {
                
                block_for_char("Sending Thread Created Sucessfully");
            }
            else {

                block_for_char("Sending Thread Creating Failed!!");
            }

            break;

        case 'j':

            if(stop_sending_thread() == 0) {
                
                block_for_char("Sending Thread Stopeed Sucessfully");
            }
            else {

                block_for_char("Sending Thread Failed to Stop!!");
            }

            break;

/*

        case 'k':

            if(start_receiving_thread() == 0) {
                
                block_for_char("Receiving Thread Created Sucessfully");
            }
            else {

                block_for_char("Receiving Thread Creating Failed!!");
            }

            break;

        case 'l':

            if(stop_receiving_thread() == 0) {
                
                block_for_char("Receiving Thread Stopeed Sucessfully");
            }
            else {

                block_for_char("Receiving Thread Failed to Stop!!");
            }

            break;
*/
        case 'm':
            performance_and_stats_monitor_control();
            //block_for_char("Not Implemented Yet!!");
            break;

        case 'n':
            if(init_40G_mac() != 0) {
                block_for_char("init_40G_mac Completed");
            }

            else {
                block_for_char("init_40G_mac failed");
            }
            break;

        case 'o':
            parse_altr_uint32("Loopback Disable (0), Loopback at Mux (1), Loopback at FIFO (2), Loopback at PHY (3).", &var_input);

            if(set_mac_loopback(var_input) != 0) {
                block_for_char("set_mac_loopback failed");
            }

            else {
                
                block_for_char("set_mac_loopback Completed");
            }

            break;

        case 'r':
            vfap_framework_rc = vfap_dma_init();

            if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {
                altera_message_print(&test_message_id, ALTERA_MESSAGE_ERROR, "[main] Error (%d) from vfap_dma_init!", vfap_framework_rc);
                block_for_char(0);
                break;
            }
            else {
                block_for_char("DMA Initialised");
            }
            break;

        case 's':
            vfap_framework_rc = vfap_dma_version(&data_out);

            if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {

                altera_message_print(&test_message_id, ALTERA_MESSAGE_ERROR, "[main] Error (%d) from vfap_dma_version!", vfap_framework_rc);
                block_for_char(0);
                break;
            }
            else {

                altera_message_print(&test_message_id, ALTERA_MESSAGE_ALL, "[main] DMA Version = %08x!", data_out);
                block_for_char("Done");
            }
            break;

        case 't':
            printf("[main]: enter test data in hex = 0x");
            scanf("%x", &data_in);

            vfap_framework_rc = vfap_dma_test(data_in, &data_out);

            if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {

                altera_message_print(&test_message_id, ALTERA_MESSAGE_ERROR, "[main] Error (%d) from vfap_dma_test!", vfap_framework_rc);
                block_for_char(0);
                break;
            }
            else {

                altera_message_print(&test_message_id, ALTERA_MESSAGE_ALL, "[main] Test Dta out = %08x!", data_out);
                block_for_char("Done");
            }

            break;

        case 'u':
            print_hardware_table(REG_VFAP_VF_MSI_X_TABLE_DEBUG, 16, 128);
            block_for_char("Access MSI-X Table Finished");
            break;
        case 'v':
            print_hardware_table(REG_VFAP_VF_MSI_X_PBA_DEBUG, 8, 128);
            block_for_char("Access MSI-X PBA Finished");
            break;

        case 'w':
            printf("[main]: enter packet length in hexadecimal bytes (0x50 - 0x2000) = ");
            scanf("%x", &length_src);
            send_individual_random_packets_pipeline();
            block_for_char("Operation Completed");
            break;

        case 'q':
            return 0;
            break;

        default:
            break;
        }

//        flush_screen();
    }

    return 0;
}


void print_header_list(void){

//    printf("\n");
    printf("\n0 - 77777xxx");
    printf("\n1 - 88888xxx");
    printf("\n2 - 99999xxx");
    printf("\n3 - aaaaaxxx");
    printf("\n4 - bbbbbxxx");
    printf("\n5 - cccccxxx");
    printf("\n6 - dddddxxx");
    printf("\n7 - eeeeexxx");
    printf("\n8 - 00 01 02 03 ...");
    printf("\n");
}


void block_for_char(const char* ss){

    char termination[32];

    if(!ss) {
        printf("\n\ntype a letter to continue >> ");
    }
    else {
        printf("\n\n%s >> ", ss);
    }
    scanf("%s", &termination[0]);

}

void  pirnt_dma_int_ack (vfap_dma_int_ack_s *ack){

    altera_message_print(&test_message_id, ALTERA_MESSAGE_ALL, "[pirnt_dma_int_ack] VF             = %x!", ack->vf);
    altera_message_print(&test_message_id, ALTERA_MESSAGE_ALL, "[pirnt_dma_int_ack] FLAG           = %x!", ack->flag);
    altera_message_print(&test_message_id, ALTERA_MESSAGE_ALL, "[pirnt_dma_int_ack] Buffer Address = %x!", ack->buffer_address);
    altera_message_print(&test_message_id, ALTERA_MESSAGE_ALL, "[pirnt_dma_int_ack] Len            = %x!", ack->len_bytes);

}

void  pirnt_rx_pkt_bytes (altr_uint32 len_bytes){

    int         i;

    altr_uint8 *pkt = &RX_buffer_bytes[0];

    altera_message_print(&test_message_id, ALTERA_MESSAGE_ALL, "[pirnt_rx_pkt] Received 0x%x bytes", len_bytes);
    altera_message_print(&test_message_id, ALTERA_MESSAGE_ALL, "[pirnt_rx_pkt] DUMP: ");

    for(i=0; i < (int) len_bytes; i++) {

        
        printf("%02x ", pkt[i]);
        //altera_message_print(&test_message_id, ALTERA_MESSAGE_ALL, "[pirnt_rx_pkt] @ %x = %x!", buffer_address + i*4, rx_packet_data[i]);
    }
    altera_message_print(&test_message_id, ALTERA_MESSAGE_ALL, "[pirnt_rx_pkt] DONE ");
}

//HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH
//HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH flush_screen HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH
//HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH
void  flush_screen(void){

    #ifdef _WIN32

        DWORD written;
        COORD curhome = {0,0};
        DWORD N;
        HANDLE hndl = GetStdHandle(STD_OUTPUT_HANDLE);
        CONSOLE_SCREEN_BUFFER_INFO csbi;

        GetConsoleScreenBufferInfo(hndl, &csbi);
        N = csbi.dwSize.X * csbi.dwCursorPosition.Y +
        csbi.dwCursorPosition.X + 1;

        FillConsoleOutputCharacter(hndl, ' ', N, curhome, &written);
        csbi.srWindow.Bottom -= csbi.srWindow.Top;
        csbi.srWindow.Top = 0;
        SetConsoleWindowInfo(hndl, TRUE, &csbi.srWindow);
        SetConsoleCursorPosition(hndl, curhome);

    #else
        system("clear");
    #endif
}

int test_init(void){

    vfap_framework_ret_status rc_vfap_framework;
    int                 i,j;
    altr_uint8          pkt_byte;
    altr_uint32         pkt_word;
    altr_uint32         *pp;

    rc_vfap_framework = vfap_framework_init();

    altera_message_init(&test_message_id, (char*) "TEST");

//    altera_message_filter(&test_message_id, ALTERA_MESSAGE_NONE); //ALTERA_MESSAGE_NONE//ALTERA_MESSAGE_ALL//ALTERA_MESSAGE_ERROR
     //vfap_framework_message_filter(ALTERA_MESSAGE_NONE);

    if(rc_vfap_framework != VFAP_FRAMEWORK_RET_SUCCESS) {

        altera_message_print(&test_message_id, ALTERA_MESSAGE_ERROR, "[test_init]: vfap_framework_init returned an error code of (%d)", rc_vfap_framework);
        block_for_char(0);
        return -1;
    }

    
    pkt_byte = 0;
    for(i=0; i<MAXIMUM_PKT_SIZE_WORDS; i++) {

        HEADER_packet_sample_words[0][i] = 0x77777000 + i;
        HEADER_packet_sample_words[1][i] = 0x88888000 + i;
        HEADER_packet_sample_words[2][i] = 0x99999000 + i;
        HEADER_packet_sample_words[3][i] = 0xAAAAA000 + i;
        HEADER_packet_sample_words[4][i] = 0xBBBBB000 + i;
        HEADER_packet_sample_words[5][i] = 0xCCCCC000 + i;
        HEADER_packet_sample_words[6][i] = 0xDDDDD000 + i;
        HEADER_packet_sample_words[7][i] = 0xEEEEE000 + i;

        HEADER_packet_sample_bytes[(i*4) + 0] = pkt_byte++;
        HEADER_packet_sample_bytes[(i*4) + 1] = pkt_byte++;
        HEADER_packet_sample_bytes[(i*4) + 2] = pkt_byte++;
        HEADER_packet_sample_bytes[(i*4) + 3] = pkt_byte++;
    }

    pkt_word = 0;
    for(i=0; i<255; i++) {
        pp = (altr_uint32*) &TX_buffer_bytes[i][0];
        for(j=0; j<MAXIMUM_PKT_SIZE_WORDS; j++) {
            pp[j] = pkt_word++;
        }
    }

    memset(&RX_buffer_bytes[0], 0, VFAP_FRAMEWORK_MAX_PACKET_LENGTH_BYTES);


    #ifdef  ALTR_OS_LINUX
    //specifiy the attribute of the pthread.
    pthread_attr_init(&pth_attr);
    pthread_attr_setdetachstate(&pth_attr, PTHREAD_CREATE_JOINABLE);
    #endif

    enable_timer_irq   = 0;
    counters_mode      = 0;
    mac_relevant_stats = 0;

    return 0;
}

void parse_altr_uint32(const char* s_string, altr_uint32 *value){

    altr_uint32 input; 

    if(!s_string){
        printf("\nEnter a number : ");
    }
    else {

        printf("\nEnter %s : ", s_string);
    }

    scanf("%u", &input);

    *value = input;
}

void print_hardware_table  (altr_uint32 base_address, altr_uint32 row_length_bytes, altr_uint32 number_of_rows) {

    int i, j;
    altr_uint32 av_data;

    for(i=0; i<(int)number_of_rows; i++) {

        printf("\n%08x : ", base_address + (i*row_length_bytes));

        for(j=0; j<(int)row_length_bytes; j+=4) {

             vfap_reg_read_32b(base_address + (i*row_length_bytes) + j ,  &av_data);

            printf("  %08x", av_data);

        }
    }
}

//HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH Sending Thread HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH
int start_sending_thread (void){
    int rc = 0;
#ifdef  ALTR_OS_LINUX

    int rt;

    if(start_sending_thread_flag) {

        rc = -1;
        printf("\nERROR; Sening Thread Is already running!\n");
    }
    else {

        rt = pthread_create(&sending_thread, &pth_attr, SendingThreadProc, 0);
            if (rt){
                printf("\nERROR; return code from pthread_create() for Sending Thread is %d\n", rt);
                rc = -1;
            }

        stop_sending_thread_flag   = 0;
        start_sending_thread_flag  = 1;
        vm_sleep(100);
    }

    return rc;
#else
    altera_message_print(&test_message_id, ALTERA_MESSAGE_ERROR, "OPERATION IS ONLY SUPPORTED IN LINUX");
    rc = -1;
    return rc;
#endif
}

int stop_sending_thread (void){
    int rc = 0;
#ifdef  ALTR_OS_LINUX

    if(!start_sending_thread_flag) {

        printf("\nERROR; Sening Thread Is not running!\n");
        rc = -1;

    }
    else {

        stop_sending_thread_flag   = 1;
        vm_sleep(100);
    }

    return rc;
#else
    altera_message_print(&test_message_id, ALTERA_MESSAGE_ERROR, "OPERATION IS ONLY SUPPORTED IN LINUX");
    rc = -1;
    return rc;
#endif
}

//HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH Receiving Thread HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH
int start_receiving_thread(void){
    int rc = 0;
#ifdef  ALTR_OS_LINUX

    int rt;

    if(start_receiving_thread_flag) {

        rc = -1;
        printf("\nERROR; Receiving Thread Is already running!\n");

    }
    else {

        rt = pthread_create(&receiving_thread, &pth_attr, ReceivingThreadProc, 0);
            if (rt){
                printf("\nERROR; return code from pthread_create() for Receiving Thread is %d\n", rt);
                //block_for_char(0);
                rc = -1;
            }

        stop_receiving_thread_flag    = 0;
        start_receiving_thread_flag   = 1;
        vm_sleep(100);
    }

    return rc;
#else
    altera_message_print(&test_message_id, ALTERA_MESSAGE_ERROR, "OPERATION IS ONLY SUPPORTED IN LINUX");
    rc = -1;
    return rc;
#endif
}

int stop_receiving_thread(void){
    int rc = 0;
#ifdef  ALTR_OS_LINUX

    if(!start_receiving_thread_flag) {

        printf("\nERROR; Receiving Thread Is not running!\n");
        rc = -1;

    }
    else {

        stop_receiving_thread_flag   = 1;
        vm_sleep(100);
    }

    return rc;
#else
    altera_message_print(&test_message_id, ALTERA_MESSAGE_ERROR, "OPERATION IS ONLY SUPPORTED IN LINUX");
    rc = -1;
    return rc;
#endif
}

//HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH Thread Implementation HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH
#ifdef  ALTR_OS_LINUX
void * SendingThreadProc  (void *thread_args){

    int rc;
    int max_loops = 1;

    int count = 0;
    
    printf("\nHello! It's me, Sending Thread!\n");


    //for(i=0; i<10; i++) {
    while(!stop_sending_thread_flag) {

        //vm_sleep(500000);

        if(1) {
            rc = send_individual_random_packets_pipeline();
        }
        else {
            rc = send_individual_random_packets();
        }

        if(rc !=0) {

            printf("Error %d in send_individual_random_packets_pipeline \n", rc);
            stop_sending_thread_flag = 1;
        }

        if(count != -1) { //not set to run enfinity
            if(count == max_loops) {
                stop_sending_thread_flag = 1;
            }
            else {
                count++;
            }
        }

        //send_individual_random_packets();
   }

   //while(!stop_sending_thread_flag) ;
   start_sending_thread_flag = 0;
   printf("\nBye Bye! It's me, Sending Thread!\n");
   pthread_exit(thread_args);

}
//======================== receving Thread =======================
void * ReceivingThreadProc(void *thread_args){
   printf("\nHello! It's me, Receiving Thread!\n");
   //Code ....









   while(!stop_receiving_thread_flag) ;
   start_receiving_thread_flag = 0;
   printf("\nBye Bye! It's me, Receiving Thread!\n");
   pthread_exit(thread_args);
}
#endif


void init_timer(void){

    #ifdef  ALTR_OS_LINUX

    /* Configure the timer to expire after 250 msec... */
    timer.it_value.tv_sec = 1;
    timer.it_value.tv_usec = 0;
    timer.it_interval.tv_sec = 1;
    timer.it_interval.tv_usec = 0;

    signal(SIGALRM, timer_irq);
    setitimer (ITIMER_REAL, &timer, NULL);

    #endif

}

void stop_timer(){

    #ifdef  ALTR_OS_LINUX


    #endif

}

void performance_and_stats_monitor_control(void){


    char sub_key= 'a';
    int quit_loop = 0;
    int i;

    //1) initialise stats
    bytes_counter = 0;
    bytes_counter_delta = 0;

    //2) initialise timer

    print_performance_and_stats_monitor();

    init_timer();
    enable_timer_irq = 1;

    while(!quit_loop){

        sub_key = getchar();

        switch(tolower(sub_key)) {

        case 'r':
            for(i=0; i<6; i++) {
                hw_acc_stats[i] = 0;
                hw_acc_stats_delta[i] = 0;
            }
            break;
        case 's':
            mac_relevant_stats = mac_relevant_stats ? 0 : 1;

            for(i=0; i<6; i++) {
                hw_acc_stats[i] = 0;
                hw_acc_stats_delta[i] = 0;
            }

            break;
        case 'm':
            counters_mode = counters_mode ? 0 : 1; 
            print_performance_and_stats_monitor();
            break;
        case 'q':
            quit_loop = 1;
            break;
        default:
            break;
        }
    }

    //4) close timer
    enable_timer_irq = 0;
    stop_timer();
}

void timer_irq (int signum) {

    int i;

    if(!enable_timer_irq) return;

    for(i=0; i<9; i++) {
        hw_acc_stats_delta[i] = hw_acc_stats[i];
    }

    if(mac_relevant_stats) {

        vfap_reg_read_32b(REG_MAC_40G_ACC_BYTES_IN,          &hw_acc_stats[0]);
        vfap_reg_read_32b(REG_MAC_40G_ACC_BACKETS_IN,        &hw_acc_stats[1]);

        vfap_reg_read_32b(REG_MAC_40G_ACC_BYTES_OUT,         &hw_acc_stats[2]);
        vfap_reg_read_32b(REG_MAC_40G_ACC_PACKETS_OUT,       &hw_acc_stats[3]);

        vfap_reg_read_32b(REG_MAC_40G_ACC_BYTES_DISCARDED,   &hw_acc_stats[4]);
        vfap_reg_read_32b(REG_MAC_40G_ACC_PACKETS_DISCARDED, &hw_acc_stats[5]);

        vfap_reg_read_32b(REG_MAC_40G_PHY_CNTR_RX_CRCERR_LO, &hw_acc_stats[6]);
        vfap_reg_read_32b(REG_MAC_40G_PHY_CNTR_RX_CRCERR_HI, &hw_acc_stats[7]);
        vfap_reg_read_32b(REG_MAC_40G_PHY_CNTR_RX_FCS      , &hw_acc_stats[8]);
    }
    else {

        vfap_reg_read_32b(REG_ACC_BASE,          &hw_acc_stats[0]);
    }

    for(i=0; i<9; i++) {
        hw_acc_stats_delta[i] = hw_acc_stats[i] - hw_acc_stats_delta[i];
    }

    print_performance_and_stats_monitor();

}

void print_performance_and_stats_monitor(void){

        flush_screen();

        printf("VM Performance and Stats monitor: \n");
        printf("__________________________\n\n");
        printf("\n\n");


        if(mac_relevant_stats) {

            printf("Out (mac tx): \n");
            printf("_______\n\n");
            printf("     Number of   %s\t:                      %u.\n"    , (counters_mode == 0) ? "Bytes   "  : "(Bytes/s)   ", (counters_mode == 0) ? hw_acc_stats[0]*16 : hw_acc_stats_delta[0]*16);
            printf("     Number of   %s\t:                      %u.\n"    , (counters_mode == 0) ? "Packets "  : "(Packets/s) ", (counters_mode == 0) ? hw_acc_stats[1]    : hw_acc_stats_delta[1]);

            printf("In (mac rx): \n");
            printf("_______\n\n");
            printf("     Number of   %s\t:                      %u.\n"    , (counters_mode == 0) ? "Bytes   "  : "(Bytes/s)   ", (counters_mode == 0) ? hw_acc_stats[2]*16 : hw_acc_stats_delta[2]*16);
            printf("     Number of   %s\t:                      %u.\n"    , (counters_mode == 0) ? "Packets "  : "(Packets/s) ", (counters_mode == 0) ? hw_acc_stats[3]    : hw_acc_stats_delta[3]);

            printf("Discards: \n");
            printf("_______\n\n");
            printf("     Number of   %s\t:                      %u.\n"    , (counters_mode == 0) ? "Bytes   "  : "(Bytes/s)   ", (counters_mode == 0) ? hw_acc_stats[4] : hw_acc_stats_delta[4]);
            printf("     Number of   %s\t:                      %u.\n"    , (counters_mode == 0) ? "Packets "  : "(Packets/s) ", (counters_mode == 0) ? hw_acc_stats[5] : hw_acc_stats_delta[5]);

            printf("PHY Stats: \n");
            printf("_______\n\n");
            printf("     CNTR_RX_CRCERR_LO (%s)\t:              %u.\n"    , (counters_mode == 0) ? "Packets "  : "(Packets/s) ", (counters_mode == 0) ? hw_acc_stats[6] : hw_acc_stats_delta[6]);
            printf("     CNTR_RX_CRCERR_HI (%s)\t:              %u.\n"    , (counters_mode == 0) ? "Packets "  : "(Packets/s) ", (counters_mode == 0) ? hw_acc_stats[7] : hw_acc_stats_delta[7]);
            printf("     CNTR_RX_FCS       (%s)\t:              %u.\n"    , (counters_mode == 0) ? "Packets "  : "(Packets/s) ", (counters_mode == 0) ? hw_acc_stats[8] : hw_acc_stats_delta[8]);

        }

        else {
            printf("ACC stats: \n");
            printf("_______\n\n");
            printf("     Number of   %s\t:                      %u.\n"    , (counters_mode == 0) ? "Bytes   "  : "(Bytes/s)   ", (counters_mode == 0) ? hw_acc_stats[0] : hw_acc_stats_delta[0]);
        }

        printf("\n");
        printf("R - Reset Counters.\n");
        printf("S - %s MAC relevant Stats.\n", (mac_relevant_stats) ? "Hide" : "Show");
        printf("M - Change Counter Mode(current = %s).\n\n", (counters_mode==0) ? "ABSOLUTE" : "RATE");
        printf("Q - Quit.\n\n");
        printf(">> \n");
}

int send_individual_random_packets(){

    int                               rc = 0;
    vfap_acc_operation_handler_s      vfap_acc_operation;
    altr_uint32                       length_dest;
    altr_uint32 static                header_id = 0;
    vfap_framework_ret_status         vfap_framework_rc;

    header_id = (header_id +1) % 8;
    //length_src = 0x200;
    length_dest = length_src;

    memset(&vfap_acc_operation, 0, sizeof(vfap_acc_operation));

    vfap_acc_operation.data.source_data.address        = (header_id == 8) ? &HEADER_packet_sample_bytes[0] : (altr_uint8*) &HEADER_packet_sample_words[header_id][0];
    vfap_acc_operation.data.source_data.len_bytes      = length_src;

    vfap_acc_operation.data.accelerated_data.address   = &RX_buffer_bytes[0];
    vfap_acc_operation.data.accelerated_data.len_bytes = length_dest;

    vfap_acc_operation.param.acc_arg0 = 0xbabecafe;
    vfap_acc_operation.param.acc_arg1 = 0xdeadbabe;
    vfap_acc_operation.param.interrupt_ack_conf = VFAP_DMA_TRANS_CONF_INTERRUPT_WHEN_RECEIVED;

    vfap_framework_rc = vfap_accelerate_my_data(&vfap_acc_operation);

    if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {
            altera_message_print(&test_message_id, ALTERA_MESSAGE_ALL, "[send_individual_random_packets] Error (%d) from vfap_accelerate_my_data!", vfap_framework_rc);
            rc = -1;
    }

    return rc;

}

int send_individual_random_packets_pipeline(){

    int                           rc = 0;

    vfap_dma_trans_s              dma_trans;
    altr_uint64                   dma_trans_id;
    altr_uint32                   sent_counter = 0;
    altr_uint32 static            header_id = 8;
    altr_uint32                   length_dest;
    altr_uint32                   max_trans_to_be_sent;
    altr_uint32                   num_deleted;
    vfap_dma_manage_trans_event_s dma_trans_event;
    vfap_framework_ret_status     vfap_framework_rc;

    //length_src = 0x200;
    length_dest = length_src;

    max_trans_to_be_sent = 255;

    //=============================================================================
    //STOP DMA FIRST
    vfap_framework_rc     = vfap_dma_stop(VFAP_DMA_TRANS_HW_LIST_GENERIC);

    if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {
        altera_message_print(&test_message_id, ALTERA_MESSAGE_ERROR, "[send_individual_random_packets_pipeline]: vfap_dma_stop returned an error code of (%d)!", vfap_framework_rc);
        return -1;
    }

    while(1) {

        sent_counter++;

        header_id = (header_id +1) % 8;
        
        memset(&dma_trans, 0, sizeof(vfap_dma_trans_s));

        dma_trans.trans_type                        = VFAP_DMA_TRANS_HOST_TO_HOST;
        dma_trans.trans_list                        = VFAP_DMA_TRANS_HW_LIST_GENERIC;

        dma_trans.source_address                    = (header_id == 8) ? &HEADER_packet_sample_bytes[0] : (altr_uint8*) &HEADER_packet_sample_words[header_id][0];
        dma_trans.source_len_bytes                  = length_src;

        dma_trans.destination_address               = &RX_buffer_bytes[0];
        dma_trans.destination_len_bytes             = length_dest;

        dma_trans.trans_param.acc_arg0              = 0xbabecafe;
        dma_trans.trans_param.acc_arg1              = 0xdeadbabe;

        dma_trans.trans_param.interrupt_ack_conf    = (sent_counter == max_trans_to_be_sent) ? VFAP_DMA_TRANS_CONF_INTERRUPT_WHEN_RECEIVED : VFAP_DMA_TRANS_CONF_ACKNOWLEDGE_NONE;

        //=============================================================================
        //Transfering DATA VIA DMA
        vfap_framework_rc     = vfap_dma_trans(&dma_trans, &dma_trans_id);

        if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {
            altera_message_print(&test_message_id, ALTERA_MESSAGE_ERROR, "[send_individual_random_packets_pipeline]: vfap_dma_trans returned an error code of (%d)!", vfap_framework_rc);
            return -1;
        }
        altera_message_print(&test_message_id, ALTERA_MESSAGE_INFO, "[send_individual_random_packets_pipeline]: Assigned ID for the trans = %d!", dma_trans_id);

        if(sent_counter == max_trans_to_be_sent) {
            break;
        }
    }

    //at this point, we would have sent max_trans_to_be_sent of trans.

    //=============================================================================
    //START DMA
    vfap_framework_rc     = vfap_dma_start(VFAP_DMA_TRANS_HW_LIST_GENERIC);

    if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {
        altera_message_print(&test_message_id, ALTERA_MESSAGE_ERROR, "[send_individual_random_packets_pipeline]: vfap_dma_start returned an error code of (%d)!", vfap_framework_rc);
        return -1;
    }

    while(1) 
    {
        memset(&dma_trans_event, 0, sizeof(vfap_dma_manage_trans_event_s));

        vfap_framework_rc = vfap_dma_trans_check_event(dma_trans_id, &dma_trans_event);

        if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {

            altera_message_print(&test_message_id, ALTERA_MESSAGE_ERROR, "[send_individual_random_packets_pipeline]: vfap_dma_trans_check_event returned an error code of (%d)!", vfap_framework_rc);
            return -1;
        }

        if(dma_trans_event.type == VFAP_DMA_MANAGE_EVENT_TYPE_DATA_TRANSFER_COMPLETE) {

            altera_message_print(&test_message_id, ALTERA_MESSAGE_INFO, "[send_individual_random_packets_pipeline]: Event = VFAP_DMA_MANAGE_EVENT_TYPE_DATA_TRANSFER_COMPLETE!");
            break; //quit while loop
        }
    }

    //data received, no delete all trans
    vfap_framework_rc = vfap_dma_trans_delete_range(sent_counter, dma_trans_id, 0, &num_deleted);

    if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {

        altera_message_print(&test_message_id, ALTERA_MESSAGE_ERROR, "[send_individual_random_packets_pipeline]: vfap_dma_trans_check_event returned an error code of (%d)!", vfap_framework_rc);
        return -1;
    }

    altera_message_print(&test_message_id, ALTERA_MESSAGE_INFO, "[send_individual_random_packets_pipeline]: %d trans were deleted!", num_deleted);

    return rc;
}


int initialise_tx_ring() {

    int rc = 0;
    vfap_framework_ret_status     vfap_framework_rc;

    vfap_dma_trans_s              dma_trans;
    altr_uint64                   dma_trans_id;
    altr_uint32                   sent_counter = 0;
    altr_uint32                   header_id = 8;
    altr_uint32                   length_dest;
    altr_uint32                   max_trans_to_be_sent;
    altr_uint32                   pattern_id = 0;

    length_dest = length_src;

    max_trans_to_be_sent = 255;

    while(1) {

        sent_counter++;

        header_id = (header_id +1) % 8;
        
        memset(&dma_trans, 0, sizeof(vfap_dma_trans_s));

        dma_trans.trans_type                        = VFAP_DMA_TRANS_HOST_TO_HOST;
        dma_trans.trans_list                        = VFAP_DMA_TRANS_HW_LIST_TRANSMIT_RING;

        dma_trans.source_address                    = &TX_buffer_bytes[pattern_id][0];
        dma_trans.source_len_bytes                  = length_src;

        dma_trans.destination_address               = 0;
        dma_trans.destination_len_bytes             = length_dest; //required to be able to get the packet back

        dma_trans.trans_param.acc_arg0              = 0xbabecafe;
        dma_trans.trans_param.acc_arg1              = 0xdeadbabe;

        dma_trans.trans_param.interrupt_ack_conf    = VFAP_DMA_TRANS_CONF_ACKNOWLEDGE_NONE;

        //=============================================================================
        //Transfering DATA VIA DMA
        vfap_framework_rc     = vfap_dma_trans(&dma_trans, &dma_trans_id);

        if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {
            altera_message_print(&test_message_id, ALTERA_MESSAGE_ERROR, "[initialise_tx_ring]: vfap_dma_trans returned an error code of (%d)!", vfap_framework_rc);
            return -1;
        }
        altera_message_print(&test_message_id, ALTERA_MESSAGE_INFO, "[initialise_tx_ring]: Assigned ID for the trans = %d!", dma_trans_id);

        if(sent_counter == max_trans_to_be_sent) {
            break;
        }
    }

    return rc;
}

int initialise_rx_ring(){

    int rc = 0;
    vfap_framework_ret_status     vfap_framework_rc;

    vfap_dma_trans_s              dma_trans;
    altr_uint64                   dma_trans_id;
    altr_uint32                   sent_counter = 0;
    altr_uint32                   header_id = 8;
    altr_uint32                   length_dest;
    altr_uint32                   max_trans_to_be_sent;

    length_dest = length_src;

    max_trans_to_be_sent = 255;

    while(1) {

        sent_counter++;

        header_id = (header_id +1) % 8;
        
        memset(&dma_trans, 0, sizeof(vfap_dma_trans_s));

        dma_trans.trans_type                        = VFAP_DMA_TRANS_HOST_TO_HOST;
        dma_trans.trans_list                        = VFAP_DMA_TRANS_HW_LIST_RECEIVE_RING;

        dma_trans.source_address                    = 0;
        dma_trans.source_len_bytes                  = 0;

        dma_trans.destination_address               = &RX_buffer_bytes[0];
        dma_trans.destination_len_bytes             = length_dest;

        dma_trans.trans_param.acc_arg0              = 0xbabecafe;
        dma_trans.trans_param.acc_arg1              = 0xdeadbabe;

        dma_trans.trans_param.interrupt_ack_conf    = VFAP_DMA_TRANS_CONF_ACKNOWLEDGE_NONE;

        //=============================================================================
        //Transfering DATA VIA DMA
        vfap_framework_rc     = vfap_dma_trans(&dma_trans, &dma_trans_id);

        if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {
            altera_message_print(&test_message_id, ALTERA_MESSAGE_ERROR, "[initialise_rx_ring]: vfap_dma_trans returned an error code of (%d)!", vfap_framework_rc);
            return -1;
        }
        altera_message_print(&test_message_id, ALTERA_MESSAGE_INFO, "[initialise_rx_ring]: Assigned ID for the trans = %d!", dma_trans_id);

        if(sent_counter == max_trans_to_be_sent) {
            break;
        }
    }

    return rc;
}

int clear_tx_ring(){

    int rc = 0;
    vfap_framework_ret_status     vfap_framework_rc;

    vfap_framework_rc = vfap_dma_clear_list          (VFAP_DMA_TRANS_HW_LIST_GENERIC);

    if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {
        altera_message_print(&test_message_id, ALTERA_MESSAGE_ERROR, "[clear_tx_ring]: vfap_dma_clear_list returned an error code of (%d)!", vfap_framework_rc);
        return -1;
    }

    return rc;

}

int clear_rx_ring(){

    int rc = 0;
    vfap_framework_ret_status     vfap_framework_rc;

    vfap_framework_rc = vfap_dma_clear_list          (VFAP_DMA_TRANS_HW_LIST_RECEIVE_RING);

    if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {
        altera_message_print(&test_message_id, ALTERA_MESSAGE_ERROR, "[clear_rx_ring]: vfap_dma_clear_list returned an error code of (%d)!", vfap_framework_rc);
        return -1;
    }

    return rc;

}

int start_dma(){

    int rc = 0;
    vfap_framework_ret_status     vfap_framework_rc;

    vfap_framework_rc = vfap_dma_configure_list          (VFAP_DMA_TRANS_HW_LIST_RECEIVE_RING, 1);

    if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {
        altera_message_print(&test_message_id, ALTERA_MESSAGE_ERROR, "[start_dma]: vfap_dma_configure_list returned an error code of (%d)!", vfap_framework_rc);
        return -1;
    }

    vfap_framework_rc = vfap_dma_configure_list          (VFAP_DMA_TRANS_HW_LIST_TRANSMIT_RING, 1);

    if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {
        altera_message_print(&test_message_id, ALTERA_MESSAGE_ERROR, "[start_dma]: vfap_dma_configure_list returned an error code of (%d)!", vfap_framework_rc);
        return -1;
    }

    vfap_framework_rc     = vfap_dma_start(VFAP_DMA_TRANS_HW_LIST_RECEIVE_RING);

    if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {
        altera_message_print(&test_message_id, ALTERA_MESSAGE_ERROR, "[start_dma]: vfap_dma_start returned an error code of (%d)!", vfap_framework_rc);
        return -1;
    }

    vfap_framework_rc     = vfap_dma_start(VFAP_DMA_TRANS_HW_LIST_TRANSMIT_RING);

    if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {
        altera_message_print(&test_message_id, ALTERA_MESSAGE_ERROR, "[start_dma]: vfap_dma_start returned an error code of (%d)!", vfap_framework_rc);
        return -1;
    }


    return rc;

}

int stop_dma(){

    int rc = 0;
    vfap_framework_ret_status     vfap_framework_rc;


    vfap_framework_rc = vfap_dma_configure_list          (VFAP_DMA_TRANS_HW_LIST_TRANSMIT_RING, 0);

    if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {
        altera_message_print(&test_message_id, ALTERA_MESSAGE_ERROR, "[stop_dma]: vfap_dma_configure_list returned an error code of (%d)!", vfap_framework_rc);
        return -1;
    }

    vfap_framework_rc = vfap_dma_configure_list          (VFAP_DMA_TRANS_HW_LIST_RECEIVE_RING, 0);

    if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {
        altera_message_print(&test_message_id, ALTERA_MESSAGE_ERROR, "[stop_dma]: vfap_dma_configure_list returned an error code of (%d)!", vfap_framework_rc);
        return -1;
    }

    vfap_framework_rc     = vfap_dma_stop(VFAP_DMA_TRANS_HW_LIST_TRANSMIT_RING);

    if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {
        altera_message_print(&test_message_id, ALTERA_MESSAGE_ERROR, "[stop_dma]: vfap_dma_stop returned an error code of (%d)!", vfap_framework_rc);
        return -1;
    }

    vfap_framework_rc     = vfap_dma_stop(VFAP_DMA_TRANS_HW_LIST_RECEIVE_RING);

    if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {
        altera_message_print(&test_message_id, ALTERA_MESSAGE_ERROR, "[stop_dma]: vfap_dma_stop returned an error code of (%d)!", vfap_framework_rc);
        return -1;
    }

    return rc;
}

int init_40G_mac() {

    int rc=0;
    return rc;
}




int set_mac_loopback(altr_uint32 enable){

    int rc = 0;
    vfap_framework_ret_status     vfap_framework_rc = VFAP_FRAMEWORK_RET_SUCCESS;
    altr_uint32 reg_dat;

    reg_dat = 0x00000000;

    if(!enable) {
        vfap_framework_rc = vfap_reg_write_32b(REG_MAC_40G_PHY_LOOPBACK, &reg_dat);
        vfap_framework_rc = vfap_reg_write_32b(REG_MAC_40G_ACC_LOOPBACK_MODES, &reg_dat);
    }

    else if(enable == 1) {

        reg_dat = enable ? 0x00000001 : 0x00000000;
        vfap_framework_rc = vfap_reg_write_32b(REG_MAC_40G_ACC_LOOPBACK_MODES, &reg_dat);

    }

    else if(enable == 2) {

        reg_dat = enable ? 0x00000002 : 0x00000000;
        vfap_framework_rc = vfap_reg_write_32b(REG_MAC_40G_ACC_LOOPBACK_MODES, &reg_dat);

    }
    else if (enable == 3) {

        reg_dat = enable ? 0x0000000F : 0x00000000;
        vfap_framework_rc = vfap_reg_write_32b(REG_MAC_40G_PHY_LOOPBACK, &reg_dat);
    }

    else {
        printf("unknown choice\r\n");
    }

    if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {
        altera_message_print(&test_message_id, ALTERA_MESSAGE_ERROR, "[set_mac_loopback]: vfap_reg_write_32b returned an error code of (%d)!", vfap_framework_rc);
        return -1;
    }

    return rc;

}
