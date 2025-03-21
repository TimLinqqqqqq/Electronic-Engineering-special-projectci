/*****************************************************************************************************************************
  This project was supported by the National Basic Research 973 Program of China under Grant No.2011CB302301
  Huazhong University of Science and Technology (HUST)   Wuhan National Laboratory for Optoelectronics

FileName: ssd.c
Author: Hu Yang		Version: 2.1	Date:2011/12/02
Description: 

History:
<contributor>     <time>        <version>       <desc>                   <e-mail>
Yang Hu	        2009/09/25	      1.0		    Creat SSDsim       yanghu@foxmail.com
2010/05/01        2.x           Change 
Zhiming Zhu     2011/07/01        2.0           Change               812839842@qq.com
Shuangwu Zhang  2011/11/01        2.1           Change               820876427@qq.com
Chao Ren        2011/07/01        2.0           Change               529517386@qq.com
Hao Luo         2011/01/01        2.0           Change               luohao135680@gmail.com
 *****************************************************************************************************************************/


#include "ssd.h"
#include <stdint.h> 
#include <math.h>
#include <inttypes.h>
#include <time.h> 
/********************************************************************************************************************************
  1，main函数中initiatio()函数用来初始化ssd,；2，make_aged()函数使SSD成为aged，aged的ssd相当于使用过一段时间的ssd，里面有失效页，
  non_aged的ssd是新的ssd，无失效页，失效页的比例可以在初始化参数中设置；3，pre_process_page()函数提前扫一遍读请求，把读请求
  的lpn<--->ppn映射关系事先建立好，写请求的lpn<--->ppn映射关系在写的时候再建立，预处理trace防止读请求是读不到数据；4，simulate()是
  核心处理函数，trace文件从读进来到处理完成都由这个函数来完成；5，statistic_output()函数将ssd结构中的信息输出到输出文件，输出的是
  统计数据和平均数据，输出文件较小，trace_output文件则很大很详细；6，free_all_node()函数释放整个main函数中申请的节点
 *********************************************************************************************************************************/
int  main()
{
    unsigned  int i,j,k;
    struct ssd_info *ssd;

#ifdef DEBUG
    printf("enter main\n"); 
#endif

    ssd=(struct ssd_info*)malloc(sizeof(struct ssd_info));
    alloc_assert(ssd,"ssd");
    memset(ssd,0, sizeof(struct ssd_info));

    ssd=initiation(ssd);
    make_aged(ssd);
    pre_process_page(ssd);

    for (i=0;i<ssd->parameter->channel_number;i++)
    {
        for (j=0;j<ssd->parameter->die_chip;j++)
        {
            for (k=0;k<ssd->parameter->plane_die;k++)
            {
                printf("%d,0,%d,%d:  %5d\n",i,j,k,ssd->channel_head[i].chip_head[0].die_head[j].plane_head[k].free_page);
            }
        }
    }
    fprintf(ssd->outputfile,"\t\t\t\t\t\t\t\t\tOUTPUT\n");
    fprintf(ssd->outputfile,"****************** TRACE INFO ******************\n");

    ssd=simulate(ssd);
    statistic_output(ssd);  
    /*	free_all_node(ssd);*/

    printf("\n");
    printf("the simulation is completed!\n");

    return 1;
    /* 	_CrtDumpMemoryLeaks(); */
}


/******************simulate() *********************************************************************
 *simulate()是核心处理函数，主要实现的功能包括
 *1,从trace文件中获取一条请求，挂到ssd->request
 *2，根据ssd是否有dram分别处理读出来的请求，把这些请求处理成为读写子请求，挂到ssd->channel或者ssd上
 *3，按照事件的先后来处理这些读写子请求。
 *4，输出每条请求的子请求都处理完后的相关信息到outputfile文件中
 **************************************************************************************************/
struct ssd_info *simulate(struct ssd_info *ssd)
{
    int flag = 0, dataset=0;
    int i = 0,j = 0;
    double output_step=0;
    unsigned int a=0,b=0;
    unsigned int count=0;
    unsigned int lsn=0;
    int device, size, ope, large_lsn;
    int64_t time_t=0;
    
   

    //errno_t err;

    printf("\n");
    printf("begin simulating.......................\n");
    printf("\n");
    printf("\n");
    printf("   ^o^    OK, please wait a moment, and enjoy music and coffee   ^o^    \n");

    ssd->tracefile = fopen(ssd->tracefilename,"r");
    if(ssd->tracefile == NULL)
    {  
        printf("the trace file can't open\n");
        return NULL;
    }

    fprintf(ssd->outputfile,"      arrive           lsn     size ope     begin time    response time    process time     channel\n");	
    fflush(ssd->outputfile);

    while(flag!=100)      
    {
	flag = 10;
	dataset = 0;
	ssd->request_count = 0;
	ssd->flag5000 = 0;
	ssd->buffer_count = 0;//Initiate buffer_count for next dataset.

	while(dataset != 1)
	{
		//開始建立新的dataset並進行處理.
		while (flag!=1)
		{
		    flag=requests_collector(ssd);//flag=1, request-collection completed. flag=10, keep collecting requests.
		    if(flag == 0 || flag == 100)//flag=0, the end of the dataset, go straight to the process(). flag=100, no data anymore, shut down.
			break;
		    if(ssd -> flag5000 == 1)//Making sure the dataset is created.
		    {
			feature_collect(ssd);
		    }
		    
		}
		
		if(flag == 1 && (ssd->request_count != ssd->buffer_count))
		{   
		    
		    get_requests(ssd,ssd->request_count);
		    if (ssd->parameter->dram_capacity!=0)
		    {
			    //printf("Debug: request_queue->first request time: %"PRId64"\n", ssd->request_queue->time);
			    buffer_management(ssd);  
			    distribute(ssd); 
		    } 
		    else
		    {
			    no_buffer_distribute(ssd);
		    }
		     
		}
		process(ssd);    
		trace_output(ssd);

		if(ssd->request_count < ssd->buffer_count)//如果dataset還沒處理完, 繼續處理. 如果處理完了就要跳開while, 初始變數並建立新的dataset.
		    ssd->request_count ++;
		//else if(ssd->request_queue != NULL)
		   // dataset = 0;
		else
		    dataset = 1;
		
	}
	
        if(flag == 0 && ssd->request_queue == NULL)
            flag = 100;
        //可考慮把演算法設計成將上一次的結果丟給下一次做預判.
        //snprintf(ssd->feature->tenant_buffer[0], sizeof(ssd->feature->tenant_buffer[0]), "%d %d",0,0);
        //snprintf(ssd->feature->tenant_buffer[1], sizeof(ssd->feature->tenant_buffer[1]),"%d %d %d %d",0,0,0,0);
    }


    fclose(ssd->tracefile);
    return ssd;
}

/****************       Requests Collector      *****************************
 Get requests that arrived already.
****************************************************************************/
int requests_collector(struct ssd_info *ssd)
{
    char buffer[200];
    unsigned int lsn=0;
    int device,  size, ope,large_lsn;
    int flag = 1;
    long filepoint; 
    int64_t time_t = 0,random_number=0;
    //int64_t final_t;
    int64_t nearest_event_time;    
    ssd->flag5000 = 0;

#ifdef DEBUG
    printf("enter get_requests,  current time:%lld\n",ssd->current_time);
#endif

    if(feof(ssd->tracefile))
        return 100; 

    //讀tracefile 並使用sscanf來解析buffer的屬性
    filepoint = ftell(ssd->tracefile);	
    fgets(buffer, 200, ssd->tracefile); //讀取200個字元到buffer中
    sscanf(buffer,"%ld %d %d %d %d",&time_t,&device,&lsn,&size,&ope);

    if ((device<0)&&(lsn<0)&&(size<0)&&(ope<0))
    {
        return 10;
    }
    if(time_t < 0)
    {
        printf("error!\n");
        while(1){}
    }
    if(feof(ssd->tracefile))
    {
        ssd->request_tail=NULL;
        return 0;
    }

    //------------  計算dataset尾端  ------------//
    if (ssd->buffer_count == 0)
    {
	int64_t random_number = (int64_t)(rand() % (8000 - 100 + 1) + 100)*100000000; //設置一隨機數從100到8000（涵蓋所有Level）
        ssd -> final_t = (int64_t)(time_t + random_number);
    }
	
    //----------------  判斷工作  -------------------//
    if(ope == 0)
        ssd->feature-> W_counts++; // 如果工作為寫, 則寫紀錄加一
    else if(ope == 1)
        ssd->feature-> R_counts++; // 如果工作為讀, 則讀紀錄加一
    else
        printf("Error bit of operation detection");
    //----------------  判斷來源  -------------------//
    if(device == 0)
        ssd->feature-> workload0++; // 如果是從workolad0來的 那workload0++
    else if(device == 1)
        ssd->feature-> workload1++; // 如果是從workload1來的 那workload1++
    else
        printf("Error bit of device");

    //------------  將資料寫入buffer ------------//
    strcpy(ssd->buffer_collect[ssd->buffer_count], buffer);

    ssd->buffer_count += 1; //紀錄目前總共進了幾筆資料, 將有利於統計dataset的大小
 
    filepoint = ftell(ssd->tracefile);	
    fgets(buffer, 200, ssd->tracefile);    //寻找下一条请求的到达时间
    sscanf(buffer,"%ld %d %d %d %d",&time_t,&device,&lsn,&size,&ope);
    ssd->next_request_time=time_t;
    fseek(ssd->tracefile,filepoint,0);      //計算檔案大小
    

    if((time_t>=ssd->final_t) || ssd->buffer_count == 8000) //Max data size of dataset: 8000
    {
        ssd->flag5000 = 1;
        return 1;
    }
    else
    {
        return 10;
    }
}
/****************       Feature Collector      *****************************
本實驗重點：將收集好的requests彙整成dataset,取得特徵（如：Level, R&W_Proportion）,按照特徵計算出Tenant的比例（模式）並將通道資訊烙印在每個request上,重新輸出給buffer_management.
Level: 整體dataset的資料數, 我們指定總共有20個Level, 且LV20的資料數為8000筆,所以每一段Level的資料差距為400筆. 
R&W_proportion: 我們將從get_request()紀錄每筆資料的資料型態（R/W）透過比例我們將可以決定每個Tenant所含之通道數量.
****************************************************************************/
int feature_collect(struct ssd_info *ssd)
{
    // 資料總數被記在"buffer_count"
    // 讀寫資料數量被記在feature_info中的"R_counts"與"W_counts".
    int i,j;
    float R_p = 0, W_p = 0, workload0_p = 0, workload1_p = 0, RW0_p = 0, RW1_p = 0, WW0_p = 0, WW1_p = 0;
    unsigned int lsn=0;
    int device, size, ope;
    int64_t time_t = 0;
    int channel_num, R_ch_num = 0, W_ch_num = 0, RW0_ch_num = 0, RW1_ch_num = 0, WW0_ch_num = 0, WW1_ch_num = 0;
    ssd->feature->level = 0;
        
    ssd->feature->level = ceil(ssd->buffer_count/400.0);
    //printf("Buffer_count: %10d\n", ssd->buffer_count);
    //printf("Level: %10d\n", ssd->feature->level);
    if(ssd->feature->level <= 10)
        ssd->feature->tenant_mode = 0;
    else if((ssd->feature->workload0)>9*(ssd->feature->workload1)) //workload1 dominant
        ssd->feature->tenant_mode = 0;
    else if(9*(ssd->feature->workload0)<(ssd->feature->workload1)) //workload0 dominant 
        ssd->feature->tenant_mode = 0;
    else
        ssd->feature->tenant_mode = 1;
    
    // 比例都要取到小數後兩位, 顯示的時候記得使用"%.2f\n"的方式才會正確顯示唷
    R_p = (float)(ssd->feature->R_counts) / (float)(ssd->buffer_count);
    R_p = (float)(round(R_p*100))/100;
    W_p = (float)(ssd->feature->W_counts) / (float)(ssd->buffer_count);
    W_p = (float)(round(W_p*100))/100;

    // Tenant Mode 0: 2 Tenants. Share the 8 channels exactly according to the proportion of R/W requests. One for READ, the other for WRITE.
    if(ssd->feature->tenant_mode == 0)
    {
        ssd->feature->R_ch = round(R_p*6) + 1; //R_ch & W_ch = 10, Error.
        ssd->feature->W_ch = round(W_p*6) + 1;
        // 將結果通道分配結果依照模式存入Buffer.
        snprintf(ssd->feature->tenant_buffer[ssd->feature->tenant_mode], sizeof(ssd->feature->tenant_buffer[ssd->feature->tenant_mode]), "%d %d",ssd->feature->R_ch, ssd->feature->W_ch);
    }
    else if(ssd->feature->tenant_mode == 1)
    {
        workload0_p = (float)(ssd->feature -> workload0) / (float)(ssd -> buffer_count);
        workload0_p = (float)(round(workload0_p*100))/100;
        workload1_p = (float)(ssd->feature -> workload1) / (float)(ssd -> buffer_count);
        workload1_p = (float)(round(workload1_p*100))/100;
	// 取得不同來源之不同工作的比例
        RW0_p = R_p * workload0_p;
        RW1_p = R_p * workload1_p;
        WW0_p = W_p * workload0_p;
        WW1_p = W_p * workload1_p;
        ssd -> feature -> RW0_ch = round(RW0_p*4) + 1;
        ssd -> feature -> RW1_ch = round(RW1_p*4) + 1;
        ssd -> feature -> WW0_ch = round(WW0_p*4) + 1;
        ssd -> feature -> WW1_ch = round(WW1_p*4) + 1;
	// 為了避免總通道數少於8或是多於8，必須針對不同情況及盡可能不影響處理速度下做處理:
        // 大於8:「輕中之重」。選擇較輕的Workload中較重的Request Type進行通道數縮減
        if(ssd -> feature -> RW0_ch+ssd -> feature -> RW1_ch+ssd -> feature -> WW0_ch+ssd -> feature -> WW1_ch > 8)
        {
            if(workload0_p > workload1_p)
            {
                //先檢查該Workload中是否有足夠的通道可以縮減
                if((ssd -> feature -> RW1_ch + ssd -> feature -> WW1_ch) > 2)
                {
                    if(RW1_p > WW1_p)
                        ssd -> feature -> RW1_ch -= 1;
                    else
                        ssd -> feature -> WW1_ch -= 1;
                }
                else //如果該Workload中的通道數不夠進行縮減，則須跟另一Workload借。
                {
                    if(RW0_p > WW0_p)
                        ssd -> feature -> RW0_ch -= 1;
                    else
                        ssd -> feature -> WW0_ch -= 1;
                }   
            }
            else
            {
                if(ssd -> feature -> RW0_ch + ssd -> feature -> WW0_ch > 2)
                {
                    if(RW0_p > WW0_p)
                        ssd -> feature -> RW0_ch -= 1;
                    else
                        ssd -> feature -> WW0_ch -= 1;
                }
                else
                {
                    if(RW1_p > WW1_p)
                        ssd -> feature -> RW1_ch -= 1;
                    else
                        ssd -> feature -> WW1_ch -= 1;
                }
            }
        }
        // 小於8:「重中之重」。選擇較重的Workload中較重的Request Type進行通道數擴增。
        else if(ssd -> feature -> RW0_ch+ssd -> feature -> RW1_ch+ssd -> feature -> WW0_ch+ssd -> feature -> WW1_ch < 8)
        {
            // printf("error tenant buffer");
            if(workload0_p > workload1_p)
            {
                if(RW0_p > WW0_p)
                    ssd -> feature -> RW0_ch += 1;
                else
                    ssd -> feature -> WW0_ch += 1;
            }
            else
            {
                if(RW1_p > WW1_p)
                    ssd -> feature -> RW1_ch += 1;
                else
                    ssd -> feature -> WW1_ch += 1;
            }
        }
        // 等於8，正常, 保持原狀態
        else
        {
            ssd -> feature -> RW0_ch = ssd -> feature -> RW0_ch;
            ssd -> feature -> RW1_ch = ssd -> feature -> RW1_ch;
            ssd -> feature -> WW0_ch = ssd -> feature -> WW0_ch;
            ssd -> feature -> WW1_ch = ssd -> feature -> WW1_ch;
        }
        // 將結果通道分配結果依照模式存入Buffer
        snprintf(ssd->feature->tenant_buffer[ssd->feature->tenant_mode], sizeof(ssd->feature->tenant_buffer[ssd->feature->tenant_mode]),"%d %d %d %d",ssd->feature->RW0_ch,ssd->feature->WW0_ch,ssd->feature->RW1_ch, ssd->feature->WW1_ch);

    }
    else
	printf("NOT DONE");
    
    // 建立初始值：由於Tenant的排序是先讀再寫（Mode_0: R/W ; Mode_1: R0/W0/R1/W1）, 又通道將以0~7的方式由左至右編排, 所以我們需要建立第二, 三及四個Tenant的通道初始值.
    W_ch_num = ssd->feature->R_ch; // 若R_ch將總共被分配x個通道, 則第[0,(x-1)]個通道都將執行讀操作, 而[x,7]個通道執行寫操作, 為此W_ch_num的初始值為：x（=R_ch）.
    WW0_ch_num = ssd->feature->RW0_ch; // 同上概念擴增至Mode_1
    RW1_ch_num = ssd->feature->RW0_ch + ssd->feature->WW0_ch;
    WW1_ch_num = ssd->feature->RW0_ch + ssd->feature->WW0_ch + ssd->feature->RW1_ch;

    for(i = 0; i < ssd->buffer_count; i++)
    {
        sscanf(ssd->buffer_collect[i], "%ld %d %d %d %d", &time_t, &device, &lsn, &size, &ope);
        if(ssd->feature ->tenant_mode == 0) // mode 0: 2 Tenants
        {
            if(ope == 0) // write
            {
                if(W_ch_num <= 7)
                {
                    device = W_ch_num;
                    W_ch_num += 1;
                }
                else
                {
                    W_ch_num = ssd->feature->R_ch;
                    device = W_ch_num;
                    W_ch_num += 1;
                }
            }
            else // read
            {
                if(R_ch_num < ssd->feature->R_ch)
                {
                    device = R_ch_num;
                    R_ch_num += 1;
                }
                else
                {
                    R_ch_num = 0;
                    device = R_ch_num;
                    R_ch_num += 1;
                }
            }
        }
        else    // mode 1: 4 Tenants
        {
            if(device == 0) // from workload 0
            {
                if(ope == 0) // write
                {
                    if(WW0_ch_num < ssd->feature->RW0_ch + ssd->feature->WW0_ch)
                    {
                        device = WW0_ch_num;
                        WW0_ch_num += 1;
                    }
                    else
                    {
                        WW0_ch_num = ssd->feature->RW0_ch;
                        device = WW0_ch_num;
                        WW0_ch_num += 1;
                    }
                }
                else // read
                {
                    if(RW0_ch_num < ssd->feature->RW0_ch)
                    {
                        device = RW0_ch_num;
                        RW0_ch_num += 1;
                    }
                    else
                    {
                        RW0_ch_num = 0;
                        device = RW0_ch_num;
                        RW0_ch_num += 1;
                    }
                }
            }
            else    // from workload 1
            {
                if(ope == 0) // write
                {
                    if(WW1_ch_num <= 7)
                    {
                        device = WW1_ch_num;
                        WW1_ch_num += 1;
                    }
                    else
                    {
                        WW1_ch_num = ssd->feature->RW0_ch + ssd->feature->WW0_ch + ssd->feature->RW1_ch;
                        device = WW1_ch_num;
                        WW1_ch_num += 1;
                    }
                }
                else // read
                {
                    if(RW1_ch_num < ssd->feature->RW0_ch + ssd->feature->WW0_ch + ssd->feature->RW1_ch)
                    {
                        device = RW1_ch_num;
                        RW1_ch_num += 1;
                    }
                    else
                    {
                        RW1_ch_num = ssd->feature->RW0_ch + ssd->feature->WW0_ch;
                        device = RW1_ch_num;
                        RW1_ch_num += 1;
                    }
                }
            }
        }
        snprintf(ssd->buffer_output[i], sizeof(ssd->buffer_output[i]), "%ld %d %d %d %d", time_t, device, lsn, size, ope);
    }
    ssd -> feature -> W_counts = 0;
    ssd -> feature -> R_counts = 0;
    ssd -> feature -> workload0 = 0;
    ssd -> feature -> workload1 = 0;
    ssd -> feature -> RW0_ch = 0;
    ssd -> feature -> RW1_ch = 0;
    ssd -> feature -> WW0_ch = 0;
    ssd -> feature -> WW1_ch = 0;
	    
    return ssd;
}

/********    get_request    ******************************************************
 *	1.add those request node to ssd->reuqest_queue
 *	return	0: reach the end of the trace
 *			-1: no request has been added
 *			1: add one request to list
 *SSD模拟器有三种驱动方式:时钟驱动(精确，太慢) 事件驱动(本程序采用) trace驱动()，
 *两种方式推进事件：channel/chip状态改变、trace文件请求达到。
 *channel/chip状态改变和trace文件请求到达是散布在时间轴上的点，每次从当前状态到达
 *下一个状态都要到达最近的一个状态，每到达一个点执行一次process
 ********************************************************************************/
int get_requests(struct ssd_info *ssd, unsigned int data_number)  
{  
    unsigned int lsn=0;
    int device,  size, ope,large_lsn;
    struct request *request1;
    int flag = 1;
    long filepoint; 
    int64_t time_t = 0;
    //int64_t final_t;
    int64_t nearest_event_time;    
    ssd->flag5000 = 0;

    sscanf(ssd->buffer_output[data_number], "%ld %d %d %d %d", &time_t, &device, &lsn, &size, &ope);//從buffer_output中掃出得到channel資訊的request

    if ((device<0)&&(lsn<0)&&(size<0)&&(ope<0))
    {
        return 100;
    }
    if (lsn<ssd->min_lsn) 
        ssd->min_lsn=lsn;
    if (lsn>ssd->max_lsn)
        ssd->max_lsn=lsn;

    ssd->current_time=time_t; 
    
    request1 = (struct request*)malloc(sizeof(struct request));
    alloc_assert(request1,"request");
    memset(request1,0, sizeof(struct request));


    request1->time = time_t; 
    request1->lsn = lsn;
    request1->size = size;
    request1->operation = ope;	
    request1->channel_assign = device;
    request1->begin_time = time_t;
    request1->response_time = 0;	
    request1->energy_consumption = 0;	
    request1->next_node = NULL;
    request1->distri_flag = 0;              // indicate whether this request has been distributed already
    request1->subs = NULL;
    request1->need_distr_flag = NULL;
    request1->complete_lsn_count = 0;         //record the count of lsn served by buffer


    if(ssd->request_queue == NULL)          //The queue is empty
    {
        ssd->request_queue = request1;
        ssd->request_tail = request1;
        ssd->request_queue_length++;
    }
    else
    {			
	(ssd->request_tail)->next_node = request1;	
	ssd->request_tail = request1;			
	ssd->request_queue_length++;
    }


    if (request1->operation==1)             //计算平均请求大小 1为读 0为写
	ssd->ave_read_size=(ssd->ave_read_size*ssd->read_request_count+request1->size)/(ssd->read_request_count+1);
		
    else
	ssd->ave_write_size=(ssd->ave_write_size*ssd->write_request_count+request1->size)/(ssd->write_request_count+1);

    return ssd;
    
}




/**********************************************************************************************************************************************
 *首先buffer是个写buffer，就是为写请求服务的，因为读flash的时间tR为20us，写flash的时间tprog为200us，所以为写服务更能节省时间
 *  读操作：如果命中了buffer，从buffer读，不占用channel的I/O总线，没有命中buffer，从flash读，占用channel的I/O总线，但是不进buffer了
 *  写操作：首先request分成sub_request子请求，如果是动态分配，sub_request挂到ssd->sub_request上，因为不知道要先挂到哪个channel的sub_request上
 *          如果是静态分配则sub_request挂到channel的sub_request链上,同时不管动态分配还是静态分配sub_request都要挂到request的sub_request链上
 *		   因为每处理完一个request，都要在traceoutput文件中输出关于这个request的信息。处理完一个sub_request,就将其从channel的sub_request链
 *		   或ssd的sub_request链上摘除，但是在traceoutput文件输出一条后再清空request的sub_request链。
 *		   sub_request命中buffer则在buffer里面写就行了，并且将该sub_page提到buffer链头(LRU)，若没有命中且buffer满，则先将buffer链尾的sub_request
 *		   写入flash(这会产生一个sub_request写请求，挂到这个请求request的sub_request链上，同时视动态分配还是静态分配挂到channel或ssd的
 *		   sub_request链上),在将要写的sub_page写入buffer链头
 ***********************************************************************************************************************************************/
struct ssd_info *buffer_management(struct ssd_info *ssd)
{   
    unsigned int j,lsn,lpn,last_lpn,first_lpn,index,complete_flag=0, state,full_page;
    unsigned int flag=0,need_distb_flag,lsn_flag,flag1=1,active_region_flag=0;           
    struct request *new_request;
    struct buffer_group *buffer_node,key;
    unsigned int mask=0,offset1=0,offset2=0;

#ifdef DEBUG
    printf("enter buffer_management,  current time:%lld\n",ssd->current_time);
#endif
    ssd->dram->current_time=ssd->current_time;
    full_page=~(0xffffffff<<ssd->parameter->subpage_page);

    new_request=ssd->request_tail;      //之前統計出來的buffer
    lsn=new_request->lsn;
    lpn=new_request->lsn/ssd->parameter->subpage_page;
    last_lpn=(new_request->lsn+new_request->size-1)/ssd->parameter->subpage_page;
    first_lpn=new_request->lsn/ssd->parameter->subpage_page;


    new_request->need_distr_flag=(unsigned int*)malloc(sizeof(unsigned int)*((last_lpn-first_lpn+1)*ssd->parameter->subpage_page/32+1));
    alloc_assert(new_request->need_distr_flag,"new_request->need_distr_flag");
    memset(new_request->need_distr_flag, 0, sizeof(unsigned int)*((last_lpn-first_lpn+1)*ssd->parameter->subpage_page/32+1));

	
    if(new_request->operation==READ) 
    {		
        while(lpn<=last_lpn)      		
        {
            /************************************************************************************************
             *need_distb_flag表示是否需要执行distribution函数，1表示需要执行，buffer中没有，0表示不需要执行
             *即1表示需要分发，0表示不需要分发，对应点初始全部赋为1
             *************************************************************************************************/
            need_distb_flag=full_page;   
            key.group=lpn;
            buffer_node= (struct buffer_group*)avlTreeFind(ssd->dram->buffer, (TREE_NODE *)&key);		// buffer node 

            while((buffer_node!=NULL)&&(lsn<(lpn+1)*ssd->parameter->subpage_page)&&(lsn<=(new_request->lsn+new_request->size-1)))             			
            {             	
                lsn_flag=full_page;
                mask=1 << (lsn%ssd->parameter->subpage_page);
                if(mask>31)
                {
                    printf("the subpage number is larger than 32!add some cases");
                    getchar(); 		   
                }
                else if((buffer_node->stored & mask)==mask)
                {
                    flag=1;
                    lsn_flag=lsn_flag&(~mask);
                }

                if(flag==1)				
                {	//如果该buffer节点不在buffer的队首，需要将这个节点提到队首，实现了LRU算法，这个是一个双向队列。		       		
                    if(ssd->dram->buffer->buffer_head!=buffer_node)     
                    {		
                        if(ssd->dram->buffer->buffer_tail==buffer_node)								
                        {			
                            buffer_node->LRU_link_pre->LRU_link_next=NULL;					
                            ssd->dram->buffer->buffer_tail=buffer_node->LRU_link_pre;							
                        }				
                        else								
                        {				
                            buffer_node->LRU_link_pre->LRU_link_next=buffer_node->LRU_link_next;				
                            buffer_node->LRU_link_next->LRU_link_pre=buffer_node->LRU_link_pre;								
                        }								
                        buffer_node->LRU_link_next=ssd->dram->buffer->buffer_head;
                        ssd->dram->buffer->buffer_head->LRU_link_pre=buffer_node;
                        buffer_node->LRU_link_pre=NULL;			
                        ssd->dram->buffer->buffer_head=buffer_node;													
                    }						
                    ssd->dram->buffer->read_hit++;					
                    new_request->complete_lsn_count++;											
                }		
                else if(flag==0)
                {
                    ssd->dram->buffer->read_miss_hit++;
                }

                need_distb_flag=need_distb_flag&lsn_flag;

                flag=0;		
                lsn++;						
            }	

            index=(lpn-first_lpn)/(32/ssd->parameter->subpage_page); 			
            new_request->need_distr_flag[index]=new_request->need_distr_flag[index]|(need_distb_flag<<(((lpn-first_lpn)%(32/ssd->parameter->subpage_page))*ssd->parameter->subpage_page));	
            lpn++;

        }
    }  
    else if(new_request->operation==WRITE)
    {
        while(lpn<=last_lpn)           	
        {	
            need_distb_flag=full_page;
            mask=~(0xffffffff<<(ssd->parameter->subpage_page));
            state=mask;

            if(lpn==first_lpn)
            {
                offset1=ssd->parameter->subpage_page-((lpn+1)*ssd->parameter->subpage_page-new_request->lsn);
                state=state&(0xffffffff<<offset1);
            }
            if(lpn==last_lpn)
            {
                offset2=ssd->parameter->subpage_page-((lpn+1)*ssd->parameter->subpage_page-(new_request->lsn+new_request->size));
                state=state&(~(0xffffffff<<offset2));
            }

            ssd=insert2buffer(ssd, lpn, state,NULL,new_request);
            lpn++;
        }
    }
    complete_flag = 1;
    for(j=0;j<=(last_lpn-first_lpn+1)*ssd->parameter->subpage_page/32;j++)
    {
        if(new_request->need_distr_flag[j] != 0)
        {
            complete_flag = 0;
        }
    }

    /*************************************************************
     *如果请求已经被全部由buffer服务，该请求可以被直接响应，输出结果
     *这里假设dram的服务时间为1000ns
     **************************************************************/
    if((complete_flag == 1)&&(new_request->subs==NULL))               
    {
        new_request->begin_time=ssd->current_time;
        new_request->response_time=ssd->current_time+1000;            
    }

    return ssd;
}
/*****************************
 *lpn向ppn的转换
 ******************************/
unsigned int lpn2ppn(struct ssd_info *ssd,unsigned int lsn)
{
    int lpn, ppn;	
    struct entry *p_map = ssd->dram->map->map_entry;
#ifdef DEBUG
    printf("enter lpn2ppn,  current time:%lld\n",ssd->current_time);
#endif
    lpn = lsn/ssd->parameter->subpage_page;			//lpn
    ppn = (p_map[lpn]).pn;
#ifdef DEBUG
	printf("lpn:%ld\tppn:%ld\n",lpn,ppn);
#endif
    return ppn;
}

/**********************************************************************************
 *读请求分配子请求函数，这里只处理读请求，写请求已经在buffer_management()函数中处理了
 *根据请求队列和buffer命中的检查，将每个请求分解成子请求，将子请求队列挂在channel上，
 *不同的channel有自己的子请求队列
 **********************************************************************************/

struct ssd_info *distribute(struct ssd_info *ssd) 
{
    unsigned int start, end, first_lsn,last_lsn,lpn,flag=0,flag_attached=0,full_page;
    unsigned int j, k, sub_size;
    int i=0;
    struct request *req;
    struct sub_request *sub;
    int* complt;

#ifdef DEBUG
    printf("enter distribute,  current time:%lld\n",ssd->current_time);
#endif
    full_page=~(0xffffffff<<ssd->parameter->subpage_page);

    req = ssd->request_tail;
    if(req->response_time != 0){
        return ssd;
    }
    if (req->operation==WRITE)
    {
        return ssd;
    }

    if(req != NULL)
    {
        if(req->distri_flag == 0)
        {
            //如果还有一些读请求需要处理
            if(req->complete_lsn_count != ssd->request_tail->size)
            {		
                first_lsn = req->lsn;				
                last_lsn = first_lsn + req->size;
                complt = req->need_distr_flag;
                start = first_lsn - first_lsn % ssd->parameter->subpage_page;
                end = (last_lsn/ssd->parameter->subpage_page + 1) * ssd->parameter->subpage_page;
                i = (end - start)/32;	

                while(i >= 0)
                {	
                    /*************************************************************************************
                     *一个32位的整型数据的每一位代表一个子页，32/ssd->parameter->subpage_page就表示有多少页，
                     *这里的每一页的状态都存放在了 req->need_distr_flag中，也就是complt中，通过比较complt的
                     *每一项与full_page，就可以知道，这一页是否处理完成。如果没处理完成则通过creat_sub_request
                     函数创建子请求。
                     *************************************************************************************/
                    for(j=0; j<32/ssd->parameter->subpage_page; j++)
                    {	
                        k = (complt[((end-start)/32-i)] >>(ssd->parameter->subpage_page*j)) & full_page;	
                        if (k !=0)
                        {
                            lpn = start/ssd->parameter->subpage_page+ ((end-start)/32-i)*32/ssd->parameter->subpage_page + j;
                            sub_size=transfer_size(ssd,k,lpn,req);    
                            if (sub_size==0) 
                            {
                                continue;
                            }
                            else
                            {
                                sub=creat_sub_request(ssd,lpn,sub_size,0,req,req->operation);
                            }	
                        }
                    }
                    i = i-1;
                }

            }
            else
            {
                req->begin_time=ssd->current_time;
                req->response_time=ssd->current_time+1000;   
            }

        }
    }
    return ssd;
}


/**********************************************************************
 *trace_output()函数是在每一条请求的所有子请求经过process()函数处理完后，
 *打印输出相关的运行结果到outputfile文件中，这里的结果主要是运行的时间
 **********************************************************************/
void trace_output(struct ssd_info* ssd){
    int flag = 1;	
    int64_t start_time, end_time;
    struct request *req, *pre_node;
    struct sub_request *sub, *tmp;

#ifdef DEBUG
    printf("enter trace_output,  current time:%lld\n",ssd->current_time);
#endif

    pre_node=NULL;
    req = ssd->request_queue;
    start_time = 0;
    end_time = 0;

    if(req == NULL)
        return;

    while(req != NULL)	
    {
        sub = req->subs;
        flag = 1;
        start_time = 0;
        end_time = 0;
        if(req->response_time != 0)
        {
	    if(ssd->request_count == 0)
	    {
		fprintf(ssd->outputfile, "  Level: %5d, Tenant Mode: %2d, Channel Distribution: %s\n", ssd->feature->level, ssd->feature->tenant_mode, ssd->feature->tenant_buffer[ssd->feature->tenant_mode]);
		fflush(ssd->outputfile);
	    }
            fprintf(ssd->outputfile,"%16ld %10d %6d %2d %16ld %16ld %10ld         %5d\n",req->time,req->lsn, req->size, req->operation, req->begin_time, req->response_time, req->response_time-req->time, req->channel_assign);
            fflush(ssd->outputfile);

            if(req->response_time-req->begin_time==0)
            {
                printf("the response time is 0?? \n");
                getchar();
            }

            if (req->operation==READ)
            {
                ssd->read_request_count++;
                ssd->read_avg=ssd->read_avg+(req->response_time-req->time);
            } 
            else
            {
                ssd->write_request_count++;
                ssd->write_avg=ssd->write_avg+(req->response_time-req->time);
            }

            if(pre_node == NULL)
            {
                if(req->next_node == NULL)
                {
                    free(req->need_distr_flag);
                    req->need_distr_flag=NULL;
                    free(req);
                    req = NULL;
                    ssd->request_queue = NULL;
                    ssd->request_tail = NULL;
                    ssd->request_queue_length--;
                }
                else
                {
                    ssd->request_queue = req->next_node;
                    pre_node = req;
                    req = req->next_node;
                    free(pre_node->need_distr_flag);
                    pre_node->need_distr_flag=NULL;
                    free((void *)pre_node);
                    pre_node = NULL;
                    ssd->request_queue_length--;
                }
            }
            else
            {
                if(req->next_node == NULL)
                {
                    pre_node->next_node = NULL;
                    free(req->need_distr_flag);
                    req->need_distr_flag=NULL;
                    free(req);
                    req = NULL;
                    ssd->request_tail = pre_node;
                    ssd->request_queue_length--;
                }
                else
                {
                    pre_node->next_node = req->next_node;
                    free(req->need_distr_flag);
                    req->need_distr_flag=NULL;
                    free((void *)req);
                    req = pre_node->next_node;
                    ssd->request_queue_length--;
                }
            }
        }
        else
        {
            flag=1;
            while(sub != NULL)
            {
                if(start_time == 0)
                    start_time = sub->begin_time;
                if(start_time > sub->begin_time)
                    start_time = sub->begin_time;
                if(end_time < sub->complete_time)
                    end_time = sub->complete_time;
                if((sub->current_state == SR_COMPLETE)||((sub->next_state==SR_COMPLETE)&&(sub->next_state_predict_time<=ssd->current_time)))	// if any sub-request is not completed, the request is not completed
                {
                    sub = sub->next_subs;
                }
                else
                {
                    flag=0;
                    break;
                }

            }

            if (flag == 1)
            {	
		if(ssd->request_count == 0)
	        {
		    fprintf(ssd->outputfile, "  Level: %5d, Tenant Mode: %2d, Channel Distribution: %s\n", ssd->feature->level, ssd->feature->tenant_mode, ssd->feature->tenant_buffer[ssd->feature->tenant_mode]);
		    fflush(ssd->outputfile);
	        }	
                fprintf(ssd->outputfile,"%16ld %10d %6d %2d %16ld %16ld %10ld         %5d\n",req->time,req->lsn, req->size, req->operation, start_time, end_time, end_time-req->time, req->channel_assign);
                fflush(ssd->outputfile);

                if(end_time-start_time==0)
                {
                    printf("the response time is 0?? \n");
                    getchar();
                }

                if (req->operation==READ)
                {
                    ssd->read_request_count++;
                    ssd->read_avg=ssd->read_avg+(end_time-req->time);
                } 
                else
                {
                    ssd->write_request_count++;
                    ssd->write_avg=ssd->write_avg+(end_time-req->time);
                }

                while(req->subs!=NULL)
                {
                    tmp = req->subs;
                    req->subs = tmp->next_subs;
                    if (tmp->update!=NULL)
                    {
                        free(tmp->update->location);
                        tmp->update->location=NULL;
                        free(tmp->update);
                        tmp->update=NULL;
                    }
                    free(tmp->location);
                    tmp->location=NULL;
                    free(tmp);
                    tmp=NULL;

                }

                if(pre_node == NULL)
                {
                    if(req->next_node == NULL)
                    {
                        free(req->need_distr_flag);
                        req->need_distr_flag=NULL;
                        free(req);
                        req = NULL;
                        ssd->request_queue = NULL;
                        ssd->request_tail = NULL;
                        ssd->request_queue_length--;
                    }
                    else
                    {
                        ssd->request_queue = req->next_node;
                        pre_node = req;
                        req = req->next_node;
                        free(pre_node->need_distr_flag);
                        pre_node->need_distr_flag=NULL;
                        free(pre_node);
                        pre_node = NULL;
                        ssd->request_queue_length--;
                    }
                }
                else
                {
                    if(req->next_node == NULL)
                    {
                        pre_node->next_node = NULL;
                        free(req->need_distr_flag);
                        req->need_distr_flag=NULL;
                        free(req);
                        req = NULL;
                        ssd->request_tail = pre_node;	
                        ssd->request_queue_length--;
                    }
                    else
                    {
                        pre_node->next_node = req->next_node;
                        free(req->need_distr_flag);
                        req->need_distr_flag=NULL;
                        free(req);
                        req = pre_node->next_node;
                        ssd->request_queue_length--;
                    }

                }
            }
            else
            {	
                pre_node = req;
                req = req->next_node;
            }
        }		
    }
}



/*******************************************************************************
 *statistic_output()函数主要是输出处理完一条请求后的相关处理信息。
 *1，计算出每个plane的擦除次数即plane_erase和总的擦除次数即erase
 *2，打印min_lsn，max_lsn，read_count，program_count等统计信息到文件outputfile中。
 *3，打印相同的信息到文件statisticfile中
 *******************************************************************************/
void statistic_output(struct ssd_info *ssd)
{
    unsigned int lpn_count=0,i,j,k,m,erase=0,plane_erase=0;
    double gc_energy=0.0;
#ifdef DEBUG
    printf("enter statistic_output,  current time:%lld\n",ssd->current_time);
#endif

    for(i=0;i<ssd->parameter->channel_number;i++)
    {
        for(j=0;j<ssd->parameter->die_chip;j++)
        {
            for(k=0;k<ssd->parameter->plane_die;k++)
            {
                plane_erase=0;
                for(m=0;m<ssd->parameter->block_plane;m++)
                {
                    if(ssd->channel_head[i].chip_head[0].die_head[j].plane_head[k].blk_head[m].erase_count>0)
                    {
                        erase=erase+ssd->channel_head[i].chip_head[0].die_head[j].plane_head[k].blk_head[m].erase_count;
                        plane_erase+=ssd->channel_head[i].chip_head[0].die_head[j].plane_head[k].blk_head[m].erase_count;
                    }
                }
                fprintf(ssd->outputfile,"the %d channel, %d chip, %d die, %d plane has : %13d erase operations\n",i,j,k,m,plane_erase);
                fprintf(ssd->statisticfile,"the %d channel, %d chip, %d die, %d plane has : %13d erase operations\n",i,j,k,m,plane_erase);
            }
        }
    }

    fprintf(ssd->outputfile,"\n");
    // fprintf(ssd->outputfile,"request queue %13ld\n",ssd->request_queue);
    // fprintf(ssd->outputfile,"request queue %p\n", (void*)ssd->request_queue);
    fprintf(ssd->outputfile,"\n");
    fprintf(ssd->outputfile,"---------------------------statistic data---------------------------\n");	 
    fprintf(ssd->outputfile,"min lsn: %13d\n",ssd->min_lsn);	
    fprintf(ssd->outputfile,"max lsn: %13d\n",ssd->max_lsn);
    fprintf(ssd->outputfile,"read count: %13ld\n",ssd->read_count);	  
    fprintf(ssd->outputfile,"program count: %13ld",ssd->program_count);	
    fprintf(ssd->outputfile,"                        include the flash write count leaded by read requests\n");
    fprintf(ssd->outputfile,"the read operation leaded by un-covered update count: %13d\n",ssd->update_read_count);
    fprintf(ssd->outputfile,"erase count: %13ld\n",ssd->erase_count);
    fprintf(ssd->outputfile,"direct erase count: %13ld\n",ssd->direct_erase_count);
    fprintf(ssd->outputfile,"copy back count: %13ld\n",ssd->copy_back_count);
    fprintf(ssd->outputfile,"multi-plane program count: %13ld\n",ssd->m_plane_prog_count);
    fprintf(ssd->outputfile,"multi-plane read count: %13ld\n",ssd->m_plane_read_count);
    fprintf(ssd->outputfile,"interleave write count: %13ld\n",ssd->interleave_count);
    fprintf(ssd->outputfile,"interleave read count: %13ld\n",ssd->interleave_read_count);
    fprintf(ssd->outputfile,"interleave two plane and one program count: %13ld\n",ssd->inter_mplane_prog_count);
    fprintf(ssd->outputfile,"interleave two plane count: %13ld\n",ssd->inter_mplane_count);
    fprintf(ssd->outputfile,"gc copy back count: %13ld\n",ssd->gc_copy_back);
    fprintf(ssd->outputfile,"write flash count: %13ld\n",ssd->write_flash_count);
    fprintf(ssd->outputfile,"interleave erase count: %13ld\n",ssd->interleave_erase_count);
    fprintf(ssd->outputfile,"multiple plane erase count: %13ld\n",ssd->mplane_erase_conut);
    fprintf(ssd->outputfile,"interleave multiple plane erase count: %13ld\n",ssd->interleave_mplane_erase_count);
    fprintf(ssd->outputfile,"read request count: %13d\n",ssd->read_request_count);
    fprintf(ssd->outputfile,"write request count: %13d\n",ssd->write_request_count);
    fprintf(ssd->outputfile,"read request average size: %13f\n",ssd->ave_read_size);
    fprintf(ssd->outputfile,"write request average size: %13f\n",ssd->ave_write_size);
    //fprintf(ssd->outputfile,"read request average response time: %lld\n",ssd->read_avg/ssd->read_request_count);
    //fprintf(ssd->outputfile,"write request average response time: %lld\n",ssd->write_avg/ssd->write_request_count);
    fprintf(ssd->outputfile,"buffer read hits: %13ld\n",ssd->dram->buffer->read_hit);
    fprintf(ssd->outputfile,"buffer read miss: %13ld\n",ssd->dram->buffer->read_miss_hit);
    fprintf(ssd->outputfile,"buffer write hits: %13ld\n",ssd->dram->buffer->write_hit);
    fprintf(ssd->outputfile,"buffer write miss: %13ld\n",ssd->dram->buffer->write_miss_hit);
    fprintf(ssd->outputfile,"erase: %13d\n",erase);
    fflush(ssd->outputfile);

    fclose(ssd->outputfile);


    fprintf(ssd->statisticfile,"\n");
    // fprintf(ssd->statisticfile,"request queue %13ld\n",ssd->request_queue);
    // fprintf(ssd->statisticfile,"request queue %p\n", (void*)ssd->request_queue);
    fprintf(ssd->statisticfile,"\n");
    fprintf(ssd->statisticfile,"---------------------------statistic data---------------------------\n");	
    fprintf(ssd->statisticfile,"min lsn: %13d\n",ssd->min_lsn);	
    fprintf(ssd->statisticfile,"max lsn: %13d\n",ssd->max_lsn);
    fprintf(ssd->statisticfile,"read count: %13ld\n",ssd->read_count);	  
    fprintf(ssd->statisticfile,"program count: %13ld",ssd->program_count);	  
    fprintf(ssd->statisticfile,"                        include the flash write count leaded by read requests\n");
    fprintf(ssd->statisticfile,"the read operation leaded by un-covered update count: %13d\n",ssd->update_read_count);
    fprintf(ssd->statisticfile,"erase count: %13ld\n",ssd->erase_count);	  
    fprintf(ssd->statisticfile,"direct erase count: %13ld\n",ssd->direct_erase_count);
    fprintf(ssd->statisticfile,"copy back count: %13ld\n",ssd->copy_back_count);
    fprintf(ssd->statisticfile,"multi-plane program count: %13ld\n",ssd->m_plane_prog_count);
    fprintf(ssd->statisticfile,"multi-plane read count: %13ld\n",ssd->m_plane_read_count);
    fprintf(ssd->statisticfile,"interleave count: %13ld\n",ssd->interleave_count);
    fprintf(ssd->statisticfile,"interleave read count: %13ld\n",ssd->interleave_read_count);
    fprintf(ssd->statisticfile,"interleave two plane and one program count: %13ld\n",ssd->inter_mplane_prog_count);
    fprintf(ssd->statisticfile,"interleave two plane count: %13ld\n",ssd->inter_mplane_count);
    fprintf(ssd->statisticfile,"gc copy back count: %13ld\n",ssd->gc_copy_back);
    fprintf(ssd->statisticfile,"write flash count: %13ld\n",ssd->write_flash_count);
    fprintf(ssd->statisticfile,"waste page count: %13ld\n",ssd->waste_page_count);
    fprintf(ssd->statisticfile,"interleave erase count: %13ld\n",ssd->interleave_erase_count);
    fprintf(ssd->statisticfile,"multiple plane erase count: %13ld\n",ssd->mplane_erase_conut);
    fprintf(ssd->statisticfile,"interleave multiple plane erase count: %13ld\n",ssd->interleave_mplane_erase_count);
    fprintf(ssd->statisticfile,"read request count: %13d\n",ssd->read_request_count);
    fprintf(ssd->statisticfile,"write request count: %13d\n",ssd->write_request_count);
    fprintf(ssd->statisticfile,"read request average size: %13f\n",ssd->ave_read_size);
    fprintf(ssd->statisticfile,"write request average size: %13f\n",ssd->ave_write_size);
    //fprintf(ssd->statisticfile,"read request average response time: %lld\n",ssd->read_avg/ssd->read_request_count);
    //fprintf(ssd->statisticfile,"write request average response time: %lld\n",ssd->write_avg/ssd->write_request_count);
    fprintf(ssd->statisticfile,"buffer read hits: %13ld\n",ssd->dram->buffer->read_hit);
    fprintf(ssd->statisticfile,"buffer read miss: %13ld\n",ssd->dram->buffer->read_miss_hit);
    fprintf(ssd->statisticfile,"buffer write hits: %13ld\n",ssd->dram->buffer->write_hit);
    fprintf(ssd->statisticfile,"buffer write miss: %13ld\n",ssd->dram->buffer->write_miss_hit);
    fprintf(ssd->statisticfile,"erase: %13d\n",erase);
//	fprintf(ssd->statisticfile,"HOT_list len: %13d\n",HotList_Size);
//	fprintf(ssd->statisticfile,"Candidate_list len: %13d\n",CandidateList_Size);

    fflush(ssd->statisticfile);

    fclose(ssd->statisticfile);
}


/***********************************************************************************
 *根据每一页的状态计算出每一需要处理的子页的数目，也就是一个子请求需要处理的子页的页数
 ************************************************************************************/
unsigned int size(unsigned int stored)
{
    unsigned int i,total=0,mask=0x80000000;

#ifdef DEBUG
    printf("enter size\n");
#endif
    for(i=1;i<=32;i++)
    {
        if(stored & mask) total++;
        stored<<=1;
    }
#ifdef DEBUG
    printf("leave size\n");
#endif
    return total;
}


/*********************************************************
 *transfer_size()函数的作用就是计算出子请求的需要处理的size
 *函数中单独处理了first_lpn，last_lpn这两个特别情况，因为这
 *两种情况下很有可能不是处理一整页而是处理一页的一部分，因
 *为lsn有可能不是一页的第一个子页。
 *********************************************************/
unsigned int transfer_size(struct ssd_info *ssd,int need_distribute,unsigned int lpn,struct request *req)
{
    unsigned int first_lpn,last_lpn,state,trans_size;
    unsigned int mask=0,offset1=0,offset2=0;

    first_lpn=req->lsn/ssd->parameter->subpage_page;
    last_lpn=(req->lsn+req->size-1)/ssd->parameter->subpage_page;

    mask=~(0xffffffff<<(ssd->parameter->subpage_page));
    state=mask;
    if(lpn==first_lpn)
    {
        offset1=ssd->parameter->subpage_page-((lpn+1)*ssd->parameter->subpage_page-req->lsn);
        state=state&(0xffffffff<<offset1);
    }
    if(lpn==last_lpn)
    {
        offset2=ssd->parameter->subpage_page-((lpn+1)*ssd->parameter->subpage_page-(req->lsn+req->size));
        state=state&(~(0xffffffff<<offset2));
    }

    trans_size=size(state&need_distribute);

    return trans_size;
}


/**********************************************************************************************************  
 *int64_t find_nearest_event(struct ssd_info *ssd)       
 *寻找所有子请求的最早到达的下个状态时间,首先看请求的下一个状态时间，如果请求的下个状态时间小于等于当前时间，
 *说明请求被阻塞，需要查看channel或者对应die的下一状态时间。Int64是有符号 64 位整数数据类型，值类型表示值介于
 *-2^63 ( -9,223,372,036,854,775,808)到2^63-1(+9,223,372,036,854,775,807 )之间的整数。存储空间占 8 字节。
 *channel,die是事件向前推进的关键因素，三种情况可以使事件继续向前推进，channel，die分别回到idle状态，die中的
 *读数据准备好了
 ***********************************************************************************************************/
int64_t find_nearest_event(struct ssd_info *ssd) 
{
    unsigned int i,j;
    int64_t time=MAX_INT64;
    int64_t time1=MAX_INT64;
    int64_t time2=MAX_INT64;

    for (i=0;i<ssd->parameter->channel_number;i++)
    {
        if (ssd->channel_head[i].next_state==CHANNEL_IDLE)
            if(time1>ssd->channel_head[i].next_state_predict_time)
                if (ssd->channel_head[i].next_state_predict_time>ssd->current_time)    
                    time1=ssd->channel_head[i].next_state_predict_time;
        for (j=0;j<ssd->parameter->chip_channel[i];j++)
        {
            if ((ssd->channel_head[i].chip_head[j].next_state==CHIP_IDLE)||(ssd->channel_head[i].chip_head[j].next_state==CHIP_DATA_TRANSFER))
                if(time2>ssd->channel_head[i].chip_head[j].next_state_predict_time)
                    if (ssd->channel_head[i].chip_head[j].next_state_predict_time>ssd->current_time)    
                        time2=ssd->channel_head[i].chip_head[j].next_state_predict_time;	
        }   
    } 

    /*****************************************************************************************************
     *time为所有 A.下一状态为CHANNEL_IDLE且下一状态预计时间大于ssd当前时间的CHANNEL的下一状态预计时间
     *           B.下一状态为CHIP_IDLE且下一状态预计时间大于ssd当前时间的DIE的下一状态预计时间
     *		     C.下一状态为CHIP_DATA_TRANSFER且下一状态预计时间大于ssd当前时间的DIE的下一状态预计时间
     *CHIP_DATA_TRANSFER读准备好状态，数据已从介质传到了register，下一状态是从register传往buffer中的最小值 
     *注意可能都没有满足要求的time，这时time返回0x7fffffffffffffff 。
     *****************************************************************************************************/
    time=(time1>time2)?time2:time1;
    return time;
}

/***********************************************
 *free_all_node()函数的作用就是释放所有申请的节点
 ************************************************/
void free_all_node(struct ssd_info *ssd)
{
    unsigned int i,j,k,l,n;
    struct buffer_group *pt=NULL;
    struct direct_erase * erase_node=NULL;
    for (i=0;i<ssd->parameter->channel_number;i++)
    {
        for (j=0;j<ssd->parameter->chip_channel[0];j++)
        {
            for (k=0;k<ssd->parameter->die_chip;k++)
            {
                for (l=0;l<ssd->parameter->plane_die;l++)
                {
                    for (n=0;n<ssd->parameter->block_plane;n++)
                    {
                        free(ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[n].page_head);
                        ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[n].page_head=NULL;
                    }
                    free(ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head);
                    ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head=NULL;
                    while(ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].erase_node!=NULL)
                    {
                        erase_node=ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].erase_node;
                        ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].erase_node=erase_node->next_node;
                        free(erase_node);
                        erase_node=NULL;
                    }
                }

                free(ssd->channel_head[i].chip_head[j].die_head[k].plane_head);
                ssd->channel_head[i].chip_head[j].die_head[k].plane_head=NULL;
            }
            free(ssd->channel_head[i].chip_head[j].die_head);
            ssd->channel_head[i].chip_head[j].die_head=NULL;
        }
        free(ssd->channel_head[i].chip_head);
        ssd->channel_head[i].chip_head=NULL;
    }
    free(ssd->channel_head);
    ssd->channel_head=NULL;

    avlTreeDestroy( ssd->dram->buffer);
    ssd->dram->buffer=NULL;

    free(ssd->dram->map->map_entry);
    ssd->dram->map->map_entry=NULL;
    free(ssd->dram->map);
    ssd->dram->map=NULL;
    free(ssd->dram);
    ssd->dram=NULL;
    free(ssd->parameter);
    ssd->parameter=NULL;

    free(ssd);
    ssd=NULL;
}


/*****************************************************************************
 *make_aged()函数的作用就死模拟真实的用过一段时间的ssd，
 *那么这个ssd的相应的参数就要改变，所以这个函数实质上就是对ssd中各个参数的赋值。
 ******************************************************************************/
struct ssd_info *make_aged(struct ssd_info *ssd)
{
    unsigned int i,j,k,l,m,n,ppn;
    int threshould,flag=0;

    if (ssd->parameter->aged==1)
    {
        //threshold表示一个plane中有多少页需要提前置为失效
        threshould=(int)(ssd->parameter->block_plane*ssd->parameter->page_block*ssd->parameter->aged_ratio);  
        for (i=0;i<ssd->parameter->channel_number;i++)
            for (j=0;j<ssd->parameter->chip_channel[i];j++)
                for (k=0;k<ssd->parameter->die_chip;k++)
                    for (l=0;l<ssd->parameter->plane_die;l++)
                    {  
                        flag=0;
                        for (m=0;m<ssd->parameter->block_plane;m++)
                        {  
                            if (flag>=threshould)
                            {
                                break;
                            }
                            for (n=0;n<(ssd->parameter->page_block*ssd->parameter->aged_ratio+1);n++)
                            {  
                                ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].page_head[n].valid_state=0;        //表示某一页失效，同时标记valid和free状态都为0
                                ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].page_head[n].free_state=0;         //表示某一页失效，同时标记valid和free状态都为0
                                ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].page_head[n].lpn=0;  //把valid_state free_state lpn都置为0表示页失效，检测的时候三项都检测，单独lpn=0可以是有效页
                                ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].free_page_num--;
                                ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].invalid_page_num++;
                                ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].blk_head[m].last_write_page++;
                                ssd->channel_head[i].chip_head[j].die_head[k].plane_head[l].free_page--;
                                flag++;

                                ppn=find_ppn(ssd,i,j,k,l,m,n);

                            }
                        } 
                    }	 
    }  
    else
    {
        return ssd;
    }

    return ssd;
}


/*********************************************************************************************
 *no_buffer_distribute()函数是处理当ssd没有dram的时候，
 *这是读写请求就不必再需要在buffer里面寻找，直接利用creat_sub_request()函数创建子请求，再处理。
 *********************************************************************************************/
struct ssd_info *no_buffer_distribute(struct ssd_info *ssd)
{
    unsigned int lsn,lpn,last_lpn,first_lpn,complete_flag=0, state;
    unsigned int flag=0,flag1=1,active_region_flag=0;           //to indicate the lsn is hitted or not
    struct request *req=NULL;
    struct sub_request *sub=NULL,*sub_r=NULL,*update=NULL;
    struct local *loc=NULL;
    struct channel_info *p_ch=NULL;


    unsigned int mask=0; 
    unsigned int offset1=0, offset2=0;
    unsigned int sub_size=0;
    unsigned int sub_state=0;

    ssd->dram->current_time=ssd->current_time;
    req=ssd->request_tail;       
    lsn=req->lsn;
    lpn=req->lsn/ssd->parameter->subpage_page;
    last_lpn=(req->lsn+req->size-1)/ssd->parameter->subpage_page;
    first_lpn=req->lsn/ssd->parameter->subpage_page;

    if(req->operation==READ)        
    {		
        while(lpn<=last_lpn) 		
        {
            sub_state=(ssd->dram->map->map_entry[lpn].state&0x7fffffff);
            sub_size=size(sub_state);
            sub=creat_sub_request(ssd,lpn,sub_size,sub_state,req,req->operation);
            lpn++;
        }
    }
    else if(req->operation==WRITE)
    {
        while(lpn<=last_lpn)     	
        {	
            mask=~(0xffffffff<<(ssd->parameter->subpage_page));
            state=mask;
            if(lpn==first_lpn)
            {
                offset1=ssd->parameter->subpage_page-((lpn+1)*ssd->parameter->subpage_page-req->lsn);
                state=state&(0xffffffff<<offset1);
            }
            if(lpn==last_lpn)
            {
                offset2=ssd->parameter->subpage_page-((lpn+1)*ssd->parameter->subpage_page-(req->lsn+req->size));
                state=state&(~(0xffffffff<<offset2));
            }
            sub_size=size(state);

            sub=creat_sub_request(ssd,lpn,sub_size,state,req,req->operation);
            lpn++;
        }
    }

    return ssd;
}



