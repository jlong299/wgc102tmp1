
/*********************************************************************************/
/* hard turbo                                                       */
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
#include "time_meas.h"

#include "ring_buffer.h" 

//need free the malloc
double cpu_freq_GHz;
FILE *  t_output_file;
FILE *  output_file;

time_stats_t time_1628t;
#define PRONUM 10
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

st_ring_buf  *alt_rd_rb0;//read from fpga
st_ring_buf  *alt_wrt_rb0;//write to fpga

typedef struct T_THREAD_INFO {
	int num;
} T_THREAD_INFO;

altera_message_handler_s        alt_message_id;

altr_uint64       *dma_trans_id_g;
int total_trans,c_trans;                


void pthread_Message( )
{
    cpu_set_t      l_cpuSet;

    CPU_ZERO( &l_cpuSet );
    printf("get affinity %d\n",pthread_getaffinity_np(pthread_self()  , sizeof( cpu_set_t ), &l_cpuSet ));
    printf("cpuset %d\n",l_cpuSet);
    printf (" thread id %u\n", pthread_self());      
    
    if ( pthread_getaffinity_np(pthread_self()  , sizeof( cpu_set_t ), &l_cpuSet ) == 0 )
	for (int i = 0; i < 4; i++)
        	if (CPU_ISSET(i, &l_cpuSet))
        		printf("XXXCPU: CPU %d\n", i);
}
    
    

int  acc_dma_mp(char* decoded_data,  //decoder output
        char* decoder_input, // input to the decoder
        int block_size, // data length
        int max_num_iter,  // the maximum number of iterations
        int crc_type,
        altr_uint64 trans_idx,
        unsigned char flint
        ) 
{
        vfap_framework_ret_status       vfap_framework_rc               = VFAP_FRAMEWORK_RET_SUCCESS;
        vfap_dma_trans_s                dma_trans;

        altr_uint64                     dma_trans_id=trans_idx;

        vfap_dma_manage_command_s     dma_manage_cmd;
        vfap_dma_manage_ack_s         dma_manage_ack;

        memset(&dma_trans,0,sizeof(vfap_dma_trans_s));
//	dma_trans.trans_list= VFAP_DMA_TRANS_HW_LIST_TRANSMIT_RING;

	dma_trans.trans_param.acc_arg0 = ( ((crc_type & 0x1) << 18)|((max_num_iter&0x1F)<<13) | (block_size & 0x1FFF) );

	  
	 // Setup DMA descriptor
        dma_trans.source_address = (altr_uint8 *)decoder_input;
#ifdef  VF4
        dma_trans.source_len_bytes = (altr_uint16)4*(block_size);//4VF
#else
        dma_trans.source_len_bytes = (altr_uint16)3*(block_size);//6VF
#endif
        dma_trans.destination_address = (altr_uint8*)decoded_data;

        dma_trans.destination_len_bytes = (altr_uint16)(block_size-4)/8+2;

        flint=1;
        //dma_trans.trans_param.interrupt_ack_conf =  (flint) ? VFAP_DMA_TRANS_CONF_INTERRUPT_WHEN_RECEIVED : VFAP_DMA_TRANS_CONF_ACKNOWLEDGE_NONE;
        dma_trans.trans_param.interrupt_ack_conf =  VFAP_DMA_TRANS_CONF_ACKNOWLEDGE_WHEN_RECEIVED;

        // Send DMA descriptor to FPGA 
        vfap_framework_rc	   = vfap_dma_trans(&dma_trans, &dma_trans_id);

        if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {
                printf("[acc_t_dma]: vfap_dma_trans returned an error code of (%d)!\n", vfap_framework_rc);
                return -1;
        }

        dma_manage_cmd.record_criteria.trans_type   = VFAP_DMA_TRANS_HOST_TO_HOST;
        dma_manage_cmd.cmd      = VFAP_DMA_MANAGE_COMMAND_START;
        vfap_framework_rc       = vfap_dma_manage(&dma_manage_cmd, &dma_manage_ack);

        if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {
                printf("[acc_dma start]: vfap_dma_manage returned an error code of (%d)!\n", vfap_framework_rc);
                return -1;
        }

//	 printf("[acc_t_dma]: Assigned ID for the trans trans_idx %d = %d! \n",trans_idx,dma_trans_id);

        return 0;
} 


int  acc_dma_dop(altr_uint64 blk_trans_id,FILE * outh_file) 
{
        vfap_framework_ret_status       vfap_framework_rc = VFAP_FRAMEWORK_RET_SUCCESS;
        vfap_dma_manage_trans_event_s dma_trans_event;
        altr_uint32                   max_allowed_trials=90000;
        altr_uint32                   times_tried = 0;
        altr_uint32                   timed_out = 0;
        altr_uint32                   data_available = 0;
        
        //	printf("\nwaitting for id %d\n",blk_trans_id);
        do{
                times_tried++;

                //sending a command to Kernel   
                vfap_framework_rc = vfap_dma_trans_check_vld(blk_trans_id, &dma_trans_event);

                if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {

                    printf("[acc_fft_dma ck event]: vfap_dma_trans_check_vld returned an error code of (%d),%ld!\n", vfap_framework_rc, blk_trans_id);
                    return VFAP_FRAMEWORK_RET_FAIL;
                }

                //=========================== Checking Acknowledgement ================================

                if(dma_trans_event.type == VFAP_DMA_MANAGE_EVENT_TYPE_DATA_TRANSFER_COMPLETE) {
                   data_available = 1;
                }
                else {
//			pthread_yield();
                    /* try again after 2 us. */
                    //vfap_framework_sleep(2);
                }

                if(times_tried >= max_allowed_trials) 
                        timed_out = 1;
             

        } while((!timed_out) && (!data_available));

        //HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH TIMED OUT OPERATION HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH

        if(timed_out) {
                vfap_framework_rc = VFAP_FRAMEWORK_RET_OPERATION_TIMED_OUT;
                printf( " VFAP_FRAMEWORK_RET_OPERATION_TIMED_OUT!\n");
                return VFAP_FRAMEWORK_RET_FAIL;
        }

  	
        vfap_framework_rc = vfap_dma_trans_sync_rx_data(blk_trans_id);

        if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {

            printf(" vfap_dma_trans_sync_rx_data returned an error code of rx (%d)!\n", vfap_framework_rc);
            return VFAP_FRAMEWORK_RET_FAIL;
        }


#if 1
        vfap_framework_rc = vfap_dma_trans_delete(blk_trans_id);
        
        if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {
        
                altera_message_print(&alt_message_id, ALTERA_MESSAGE_ERROR, "[vfap_dma_manage_wait_for_data]: vfap_dma_trans_destroy returned an error code of (%d)!", vfap_framework_rc);
                return VFAP_FRAMEWORK_RET_FAIL;
        }
        
        altera_message_print(&alt_message_id, ALTERA_MESSAGE_INFO, "[vfap_dma_manage_wait_for_data]: Exit point!");
#endif
        return VFAP_FRAMEWORK_RET_SUCCESS;
}


int alt_hrd_turbo_dma_p(char* decoded_data,  //decoder output
	char* decoder_input, // input to the decoder
	int block_size, // data length
	int max_num_iter,  // the maximum number of iterations
        int crc_type,
        altr_uint64 trans_idx,
        FILE *	output_file
)
{
        uint32_t  ept; 
       	while(1)
        {
                vfap_dma_buf_check_empty(&ept);
                       //printf("\n wait for fuf %d",ept);
                if(1==ept)
                        break;
		else pthread_yield();
        }   

        return acc_dma_mp(decoded_data,decoder_input,block_size,max_num_iter,crc_type,trans_idx,0);
       
}

/* Find difference in time */
struct timespec alt_diff_time(struct timespec start, struct timespec end)
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



void*   alt_buf_write(void* q)
{	
	st_elems tst_ele_in;
	st_elems tst_ele_file[18];
        char *decoded_data;
        char *decoder_input;
        char *decoder_inp;
        int readdata[3];
        int block_size = 40;
        int max_num_iter = 8;
        int i,cnt=0;
        int crc_type = 0;

        FILE * blksize_file;
        FILE * input_file;
        char * line = NULL;
        size_t len = 0;
        ssize_t read;
        int cfile,k;
long ct;
	printf("\n feed the data in now .................................\n");

        blksize_file=fopen("ctc_input_info_0.txt","r");

        if(!blksize_file)
          printf("Error in reading blksize file!");


        input_file=fopen("ctc_input_data_0.txt","r");

        if(!input_file)
          printf("Error in reading ctc_data_input file!");	  

        cfile=0;
        while ((read = getline(&line, &len, blksize_file)) != -1 ){
                sscanf( line, "%d %d %d", &block_size,&max_num_iter,&crc_type);


                decoder_input = (char *)calloc( block_size*8 ,sizeof(char));

                if (decoder_input == NULL)
                {
                  printf("Out of memory!\n");
                //exit();
                 
                }

                decoded_data = (char *)calloc( block_size*8 ,sizeof(char) );

                if (decoded_data == NULL)
                {
                  printf("Out of memory!\n");
                  //exit();
                }

                decoder_inp=decoder_input;

                for ( i=0;i < block_size;i++ )
                {
                         if((read = getline(&line, &len,input_file)) == -1) 	  
                                break;

                         sscanf( line, "%d %d %d",	&readdata[0], &readdata[1],&readdata[2] );

                        //			  *decoder_inp++ = 0;
                          *decoder_inp++ =readdata[0];
                          *decoder_inp++ =readdata[1];
                          *decoder_inp++ =readdata[2];			  
                 
                }   


                	  tst_ele_file[cfile].block_size=block_size;
                	  tst_ele_file[cfile].crc_type=crc_type;
                	  tst_ele_file[cfile].max_num_iter=max_num_iter;
                	  tst_ele_file[cfile].decoder_input=decoder_input;
                	  tst_ele_file[cfile].decoded_data=decoded_data;
                	  cfile++;
        	  
          }

          fclose(blksize_file);
          fclose(input_file);

k=0;
         
           for(ct=0;ct<10000000;ct++)
        	{	
   
   //     while(1)
        {

                if(!alt_rb_full(alt_wrt_rb0)) {
                	tst_ele_in.block_size=tst_ele_file[k].block_size;
                	tst_ele_in.crc_type=tst_ele_file[k].crc_type;
                	tst_ele_in.max_num_iter=tst_ele_file[k].max_num_iter;
                	tst_ele_in.decoder_input=tst_ele_file[k].decoder_input;
                	tst_ele_in.decoded_data=tst_ele_file[k].decoded_data;
                        reset_meas(&tst_ele_in.dltatime);
                        start_meas(&tst_ele_in.dltatime);
                	alt_rb_write(alt_wrt_rb0, &tst_ele_in,1);   
                	cnt++;	
                        k++;
                        if(cfile-1==k) {k=0; //break;// sleep(1);
			}
                	//printf("\n %d init else in\n",k);
                       }
                else pthread_yield();

		if(0==ct%50)sleep(1);  
        }
                }
                /* if (decoder_input != NULL)
                free(decoder_input);
                if (decoded_data != NULL)
                free(decoded_data);
                */
        return  0;

}

void*  alt_buf_read(void* q)
{
	st_elems tst_ele;
        int block_size;int r;//char *pinhex;
	
	printf("\n Get the result standby now ...................................\n");
        sleep(5);
	while(1){
                r=alt_rb_read(alt_rd_rb0, &tst_ele);

#if 1
                if(READ_FINISH==r){                               
		//printf("\n wait for sn %d \n",tst_ele.sn);		
                        while(1){
				if(VFAP_FRAMEWORK_RET_SUCCESS==acc_dma_dop(tst_ele.sn, output_file))
				{
					//printf("\n now get the %lld ele sn\n",tst_ele.sn);			
                                        stop_meas(&tst_ele.dltatime);  

					break;
				}
			        else        pthread_yield();
                        }        		
                        
                        
			block_size=tst_ele.block_size;	
			//pinhex=tst_ele.decoder_input;			
                        
                        printf("every turbo take:%f us blk size %d.\n",(double)tst_ele.dltatime.diff/cpu_freq_GHz/1000.0,block_size);//500 would panic the system

//                        printf("\n now break after get the %d ele sn\n",tst_ele.sn);
#if 0  

			fprintf(output_file, "\n Block size under testing: %d max iter %d crc %d ,count %lld",block_size, tst_ele.max_num_iter, tst_ele.crc_type,tst_ele.sn);
 
			fprintf(output_file,"\n input data in hex format\n");
			
			len= block_size*4;
                        
			for(k=0;k<len;k++)
			{
			
			        fprintf(output_file,"%x  ",*pinhex++);
			}  
			
			fprintf(output_file,"\n input data in dec format\n");
			pinhex=tst_ele.decoder_input;
			
			for(k=0;k<len;k++)
			{
				pint=*pinhex++;
				fprintf(output_file,"%d  ",pint);
			}          		
                        pout=tst_ele.decoded_data;

                        len=(block_size-4)/8;
                        fprintf(output_file,"\n out data in hex format	%lld:\n",tst_ele.sn);
                        for(k=0;k<len;k++)
                        {
                        	fprintf(output_file,"%x  ",*pout++);
                        }
                        fprintf(output_file,"\n outdata  end >>\n");

                        //fprintf(t_output_file,"\n out data in hex format	%d:\n",len);
                        pout=tst_ele.decoded_data;
                        for(i = 0; i < len; i++)
                        {
                                prc=(pout)[i];
                                for(k = 0; k <= 7; k++)
                                {		   
                                        fprintf(t_output_file,"%d\n", ((prc & (1<<k)) ? 1:0));

                                }
                        }
		   
                        // fprintf(t_output_file,"t output file sn %lld %s\n",tst_ele.sn," sn  end");
                        fflush(t_output_file);
     
#endif


              	}
#endif
            	else 
			pthread_yield();
        }

        fclose(t_output_file);
        return 0;

}



void*  alt_hturbo_deam(void *p)
{

        st_elems *pele=(st_elems *)malloc(SIZE_ELEMS);//malloc failure?
        FILE *	toutput_file;
        int repeat_no=0;
        //int k=0;
        //struct timespec start, end, time_taken;
        //double time_taken_sec, time_taken_nsec, total_time_taken_usec, time_per_usec;
        //int blksiz;



	printf("\n turbo deam standby now ..................................\n");
        sleep(5);
	toutput_file=fopen("alt_hdecoded_time.txt","wt");

        if(!toutput_file)
                printf("Error in writing hdecoded_time!");

        //reset_meas(&time_1628t);

        while(1)
        {		
                if(READ_ERR==alt_rb_read(alt_wrt_rb0, pele)){
                        //fprintf(output_file," read err\n");
                        //if(++k >20000000) break;
                        pthread_yield();
                }
                else{
                        repeat_no++;
                
                	//if(0==repeat_no%1000)
                	//	    clock_gettime(CLOCK_MONOTONIC, &start);
                	//blksiz=pele->block_size;
                	//start_meas(&time_1628t);   
                	printf("\n start meas for sn %lld",pele->sn);
                	reset_meas(&(pele->dltatime));
                	start_meas(&(pele->dltatime));

	        	if(alt_hrd_turbo_dma_p(pele->decoded_data,pele->decoder_input,pele->block_size,/*decoder_type, num_engines,*/ pele->max_num_iter, /*bit_width, en_et,*/ pele->crc_type,pele->sn,/* crc_reflect_out,*/(FILE *)toutput_file)!=0)
				break;
                	//stop_meas(&time_1628t);
                	//printf("every turbo take:%f us blk size %d.\n",(double)time_1628t.diff/cpu_freq_GHz/1000.0/300,blksiz);//500 would panic the system
                	//if(0==repeat_no%1000){
                	/*	clock_gettime(CLOCK_MONOTONIC, &end);
                	time_taken				= alt_diff_time(start,end);
                	time_taken_sec			= (double)time_taken.tv_sec;
                	time_taken_nsec 		= (double)time_taken.tv_nsec;
                	time_per_usec	= ((time_taken_sec * 1000000) + (time_taken_nsec / 1000) );

                	printf("\n ACC took %03.3f microseconds to process  a single turbo\n", time_per_usec);

                */
                	while(1)
                	{
                        	if(!alt_rb_full(alt_rd_rb0)) 
                                	break;
                        	else pthread_yield();
                	}
                	alt_rb_write(alt_rd_rb0,pele,0);
                //}
                	pthread_yield();
          	}
        }
        fclose(toutput_file);
        return 0;
}


int  alt_buf_init(unsigned int sz_buf)
{

        alt_rd_rb0 = (st_ring_buf *) malloc(sizeof(st_ring_buf));
        alt_wrt_rb0 = (st_ring_buf *) malloc(sizeof(st_ring_buf));
        alt_init_rb(alt_rd_rb0, sz_buf);
        alt_init_rb(alt_wrt_rb0, sz_buf*2);
        return  0;
}



void alt_turbo_example()
{
     
        pthread_t thread1, thread2, thread3;
        T_THREAD_INFO thdinfo[PRONUM];
        int count = 0;
        int thread1Create, thread2Create, thread3Create, i, temp;

        struct sched_param param;
        
    	cpu_set_t cpu1, cpu2, cpu3;
        pthread_attr_t attr;
         
        pthread_attr_init(&attr);      
        cpu_freq_GHz=get_cpu_freq_GHz();
        printf("\n cpu freg GHZ %f",cpu_freq_GHz);     
        //sleep(5);
	output_file=fopen("alt_hdecoded_output_debug.txt","wt");
	if(!output_file)
		printf("Error in writing hdecoded_output_debug!");	
	t_output_file=fopen("alt_hdecoded_output.txt", "wt");	
	if(!t_output_file)
		printf("Error in writing hdecoded_output!");
        
        alt_buf_init(50); 
       
        thdinfo[1].num = 1;
        param.sched_priority = 80;
        pthread_attr_setschedpolicy(&attr,SCHED_RR);
        pthread_attr_setschedparam(&attr,&param);
        pthread_attr_setinheritsched(&attr,PTHREAD_EXPLICIT_SCHED);
        
        thread1Create=pthread_create(&thread1, &attr, alt_buf_write, &thdinfo[1]);

        thread2Create=pthread_create(&thread2, &attr, alt_hturbo_deam, NULL);

        thdinfo[0].num = 0;
        param.sched_priority = 80;
        pthread_attr_setschedpolicy(&attr,SCHED_RR);
        pthread_attr_setschedparam(&attr,&param);
        pthread_attr_setinheritsched(&attr,PTHREAD_EXPLICIT_SCHED);
        
        thread3Create=pthread_create(&thread3, &attr, alt_buf_read, &thdinfo[0]);


	CPU_ZERO(&cpu1);
    	CPU_SET(1, &cpu1);
    	temp = pthread_setaffinity_np(thread1, sizeof(cpu_set_t), &cpu1);
    	printf("setaffinity=%d\n", temp);
    	printf("Set returned by pthread_getaffinity_np() contained:\n");
    	for (i = 0; i < CPU_SETSIZE; i++)
        	if (CPU_ISSET(i, &cpu1))
            		printf("CPU1: CPU %d\n", i);

    	CPU_ZERO(&cpu2);
    	CPU_SET(2, &cpu2);
    	temp = pthread_setaffinity_np(thread2, sizeof(cpu_set_t), &cpu2);
    	for (i = 0; i < CPU_SETSIZE; i++)
        	if (CPU_ISSET(i, &cpu2))
            		printf("CPU2: CPU %d\n", i);

    	CPU_ZERO(&cpu3);
    	CPU_SET(3, &cpu3);
    	temp = pthread_setaffinity_np(thread3, sizeof(cpu_set_t), &cpu3);
    	for (i = 0; i < CPU_SETSIZE; i++)
        	if (CPU_ISSET(i, &cpu3))
            		printf("CPU3: CPU %d\n", i);


    	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpu1);
    
    
    
         // pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpu1);
    
    	pthread_join(thread1, NULL);
    	pthread_join(thread2, NULL);
    	pthread_join(thread3, NULL);
        

        
        alt_del_rb(alt_rd_rb0);    
        alt_del_rb(alt_wrt_rb0);
        pthread_attr_destroy(&attr);


}




