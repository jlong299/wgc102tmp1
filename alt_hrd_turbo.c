

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
#include "ring_buffer.h" 

#include <time.h>
#include "time_meas.h"


//need free the malloc
time_stats_t time_1628t;
#define PRONUM 10

altera_message_handler_s        alt_message_id;


st_ring_buf  *alt_rd_rb0;//read from fpga
st_ring_buf  *alt_wrt_rb0;//write to fpga


typedef struct T_THREAD_INFO {
	int num;
} T_THREAD_INFO;





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
   
	   FILE *						blksize_file;
	   FILE *						input_file;
	   char * line = NULL;
	   size_t len = 0;
	   ssize_t read;
       int ct,cfile,k;



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
		  tst_ele_file[cfile].sn=cnt;
		  cfile++;
		  
	  }
   
	  fclose(blksize_file);
	  fclose(input_file);


	 
 //   for(ct=0;ct<1000000;ct++)
   	{	
		for(k=0;k<cfile;k++)
		{
			tst_ele_in.block_size=tst_ele_file[k].block_size;
			tst_ele_in.crc_type=tst_ele_file[k].crc_type;
			tst_ele_in.max_num_iter=tst_ele_file[k].max_num_iter;
			tst_ele_in.decoder_input=tst_ele_file[k].decoder_input;
			tst_ele_in.decoded_data=tst_ele_file[k].decoded_data;
			tst_ele_in.sn=cfile;
			alt_rb_write(alt_wrt_rb0, &tst_ele_in);   
			cnt++;	
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
    int len,block_size,i;
	char *pout;

	char prc;
	FILE *	t_output_file;
	FILE *	output_file;
	int r;
	int k=0;	
	char *pinhex;
	int pint;

	output_file=fopen("alt_hdecoded_output_debug.txt","wt");
	if(!output_file)
		printf("Error in writing hdecoded_output_debug!");	
	t_output_file=fopen("alt_hdecoded_output.txt", "wt");	
	if(!t_output_file)
		printf("Error in writing hdecoded_output!");

	
	while(1){
		
    	r=alt_rb_read(alt_rd_rb0, &tst_ele);    
    
#if 1
      if(READ_FINISH==r){

			
			block_size=tst_ele.block_size;
			

			pinhex=tst_ele.decoder_input;			
			
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
	 	  
      	}
#endif
    	sched_yield();
		}

	  fclose(t_output_file);
	  return 0;



}


void alt_hrd_turbo_dma(char* decoded_data,  //decoder output
	char* decoder_input, // input to the decoder
	int block_size, // data length
	int max_num_iter,  // the maximum number of iterations
    int crc_type,
    unsigned char flint,
    FILE *	output_file
)
{

    vfap_framework_ret_status       vfap_framework_rc               = VFAP_FRAMEWORK_RET_SUCCESS;
    vfap_dma_trans_s                dma_trans;
    
 
    
    vfap_dma_manage_trans_event_s dma_trans_event;
    vfap_dma_manage_command_s     dma_manage_cmd;
    vfap_dma_manage_ack_s         dma_manage_ack;
    
    altr_uint32                   max_allowed_trials=100000;
    altr_uint32                   times_tried = 0;
    altr_uint32                   timed_out = 0;
    altr_uint32                   data_available = 0;
   
    altr_uint64                     blk_trans_id;
 
    altr_uint64                     dma_trans_id;
    
    memset(&dma_trans,0,sizeof(vfap_dma_trans_s));


   // dma_trans.trans_type            = VFAP_DMA_TRANS_HOST_TO_HOST;
  //  dma_trans.trans_list            = VFAP_DMA_TRANS_HW_LIST_GENERIC;


    dma_trans.trans_param.acc_arg0              = ( ((crc_type & 0x1) << 18)|((max_num_iter&0x1F)<<13) | (block_size & 0x1FFF) );  
    //    dma_trans.trans_param.acc_arg0 = 0xbabecafe;
   //dma_trans.trans_param.acc_arg1 = 0xdeadbabe;
    
    
    // Setup DMA descriptor
    dma_trans.source_address        = (altr_uint8 *)decoder_input;
    dma_trans.source_len_bytes      = (altr_uint16)3*(block_size);
    
    dma_trans.destination_address   = (altr_uint8*)decoded_data;

    dma_trans.destination_len_bytes = (altr_uint16)(block_size-4)/8;

	//dma_trans.trans_param.interrupt_ack_conf = VFAP_DMA_TRANS_CONF_INTERRUPT_WHEN_RECEIVED;
	flint=1;
    dma_trans.trans_param.interrupt_ack_conf =  (flint) ? VFAP_DMA_TRANS_CONF_INTERRUPT_WHEN_RECEIVED : VFAP_DMA_TRANS_CONF_ACKNOWLEDGE_NONE;


	//if output_file?
	fprintf(output_file, "\n acc_arg0 0x%x %dsource_len_bytes  %d destination_len_bytes %d ",dma_trans.trans_param.acc_arg0,dma_trans.trans_param.acc_arg0, dma_trans.source_len_bytes, dma_trans.destination_len_bytes);


    // Send DMA descriptor to FPGA 
    vfap_framework_rc     = vfap_dma_trans(&dma_trans, &dma_trans_id);
    
    if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {
        altera_message_print(&alt_message_id, ALTERA_MESSAGE_ERROR, "[acc_t_dma]: vfap_dma_trans returned an error code of (%d)!", vfap_framework_rc);
        exit(EXIT_FAILURE);
    }
    altera_message_print(&alt_message_id, ALTERA_MESSAGE_INFO, "[acc_t_dma]: Assigned ID for the trans = %d!", dma_trans_id);


    /* Instructing Accelerator to START processing Descriptors */
    dma_manage_cmd.cmd      = VFAP_DMA_MANAGE_COMMAND_START;
    vfap_framework_rc       = vfap_dma_manage(&dma_manage_cmd, &dma_manage_ack);
    
    if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {
    
        altera_message_print(&alt_message_id, ALTERA_MESSAGE_ERROR, "[acc_t_dma]: vfap_dma_manage returned an error code of (%d)!", vfap_framework_rc);
        exit(EXIT_FAILURE);
    }



    do{
        times_tried++;

  

        blk_trans_id  = dma_trans_id;
 

        //sending a command to Kernel   

        vfap_framework_rc = vfap_dma_trans_check_event(blk_trans_id, &dma_trans_event);

        if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {

            altera_message_print(&alt_message_id, ALTERA_MESSAGE_ERROR, "[acc_t_dma]: vfap_dma_trans_check_event returned an error code of (%d)!", vfap_framework_rc);
            exit(EXIT_FAILURE);
        }


        if(dma_trans_event.type == VFAP_DMA_MANAGE_EVENT_TYPE_DATA_TRANSFER_COMPLETE) {

            data_available = 1;
        }
        else {
            /* try again after 2 us. */
            //vfap_framework_sleep(2);
        }

        if(times_tried >= max_allowed_trials) {
            timed_out = 1;
        }

    } while((!timed_out) && (!data_available));


    if(timed_out) {

        vfap_framework_rc = VFAP_FRAMEWORK_RET_OPERATION_TIMED_OUT;
        altera_message_print(&alt_message_id, ALTERA_MESSAGE_ERROR, "[acc_t_dma]: VFAP_FRAMEWORK_RET_OPERATION_TIMED_OUT!");
        exit(EXIT_FAILURE);
    }

    /* Instructing driver to copy ALL blocks output data from Kernel memory into output arrays in user space. */


        blk_trans_id=dma_trans_id;
 

        vfap_framework_rc = vfap_dma_trans_sync_rx_data(blk_trans_id);

        if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {

            altera_message_print(&alt_message_id, ALTERA_MESSAGE_ERROR, "[acc_t_dma]: vfap_dma_trans_sync_rx_data returned an error code of (%d)!", vfap_framework_rc);
            exit(EXIT_FAILURE);
        }

		vfap_framework_rc = vfap_dma_trans_delete(blk_trans_id);

		if(vfap_framework_rc != VFAP_FRAMEWORK_RET_SUCCESS) {

			altera_message_print(&alt_message_id, ALTERA_MESSAGE_ERROR, "[vfap_dma_manage_wait_for_data]: vfap_dma_trans_destroy returned an error code of (%d)!", vfap_framework_rc);
			 exit(EXIT_FAILURE);
		}

		altera_message_print(&alt_message_id, ALTERA_MESSAGE_INFO, "[vfap_dma_manage_wait_for_data]: Exit point!");

}



void*  alt_hturbo_deam(void *p)
{

    st_elems *pele=(st_elems *)malloc(SIZE_ELEMS);//malloc failure?
    FILE *	output_file;
	
	int repeat_no=0;
    int k=0;
    struct timespec                        start, end, time_taken;
    double                          time_taken_sec, time_taken_nsec, total_time_taken_usec, time_per_usec;

    unsigned char fint=0;

    double cpu_freq_GHz;
    cpu_freq_GHz=get_cpu_freq_GHz();
	output_file=fopen("alt_hdecoded_time.txt","wt");
		
		if(!output_file)
			printf("Error in writing hdecoded_time!");
		
    reset_meas(&time_1628t);
	
	while(1)
	{		
	    if(READ_ERR==alt_rb_read(alt_wrt_rb0, pele)){
			//fprintf(output_file," read err\n");
			//if(++k >20000000) break;
			continue;
	    	}
		else{
			repeat_no++;
		
			//printf(" read OK\n");
			}
		if(!repeat_no%10) fint=1;
	        //if(0==repeat_no%1000)
        	    clock_gettime(CLOCK_MONOTONIC, &start);
	start_meas(&time_1628t);        	
                alt_hrd_turbo_dma(pele->decoded_data,pele->decoder_input,pele->block_size,/*decoder_type, num_engines,*/ pele->max_num_iter, /*bit_width, en_et,*/ pele->crc_type,/* crc_reflect_out,*/fint,(FILE *)output_file);
        
       stop_meas(&time_1628t);
	  
       //printf("16to8 take:%f us \n",(double)time_1628t.diff/cpu_freq_GHz/1000.0);
	        //if(0==repeat_no%1000){
		clock_gettime(CLOCK_MONOTONIC, &end);
		time_taken				= alt_diff_time(start,end);
		time_taken_sec			= (double)time_taken.tv_sec;
		time_taken_nsec 		= (double)time_taken.tv_nsec;
		time_per_usec	= ((time_taken_sec * 1000000) + (time_taken_nsec / 1000) );
		
		printf("\n ACC took %03.3f microseconds to process  a single turbo\n", time_per_usec);
  

                alt_rb_write(alt_rd_rb0,pele);

		//}
	

		
		sched_yield();

	}
	fclose(output_file);
	return 0;
}


int  alt_buf_init(unsigned int sz_buf)
{

    alt_rd_rb0 = (st_ring_buf *) malloc(sizeof(st_ring_buf));
    alt_wrt_rb0 = (st_ring_buf *) malloc(sizeof(st_ring_buf));

    alt_init_rb(alt_rd_rb0, sz_buf);
    alt_init_rb(alt_wrt_rb0, sz_buf);


	return  0;
}



void alt_turbo_example()
{

    pthread_t ap[PRONUM];
    
    T_THREAD_INFO thdinfo[PRONUM];
    
    pthread_t crypt;
    
    int count = 0;
    


	alt_buf_init(1024);
    
    thdinfo[0].num = 0;
    pthread_create(&ap[0], NULL, alt_buf_write, &thdinfo[0]);
    

    thdinfo[1].num = 1;
    pthread_create(&ap[1], NULL, alt_buf_read, &thdinfo[1]);
    	
    pthread_create(&crypt, NULL, alt_hturbo_deam, NULL);
	
    for (count = 0; count < 2; count++)
    {
    	 pthread_join(ap[count], NULL);
    }
        
    pthread_join(crypt, NULL);    
    
    alt_del_rb(alt_rd_rb0);    
    
    alt_del_rb(alt_wrt_rb0);


}





