
#include "ReadSPS.h"

#include <stdio.h>  
#include <stdint.h>  
#include <string.h>  
#include <math.h>  

typedef  unsigned int UINT;  
typedef  unsigned char BYTE;  
typedef  unsigned long DWORD;  
  
UINT Ue(BYTE *pBuff, UINT nLen, UINT &nStartBit)  
{  
    //计算0bit的个数  
    UINT nZeroNum = 0;  
    while (nStartBit < nLen * 8)  
    {  
        if (pBuff[nStartBit / 8] & (0x80 >> (nStartBit % 8))) //&:按位与，%取余  
        {  
            break;  
        }  
        nZeroNum++;  
        nStartBit++;  
    }  
    nStartBit ++;  
  
  
    //计算结果  
    DWORD dwRet = 0;  
    for (UINT i=0; i<nZeroNum; i++)  
    {  
        dwRet <<= 1;  
        if (pBuff[nStartBit / 8] & (0x80 >> (nStartBit % 8)))  
        {  
            dwRet += 1;  
        }  
        nStartBit++;  
    }  
    return (1 << nZeroNum) - 1 + dwRet;  
}  
  
  
int Se(BYTE *pBuff, UINT nLen, UINT &nStartBit)  
{  
    int UeVal=Ue(pBuff,nLen,nStartBit);  
    double k=UeVal;  
    int nValue=ceil(k/2);//ceil函数：ceil函数的作用是求不小于给定实数的最小整数。ceil(2)=ceil(1.2)=cei(1.5)=2.00  
    if (UeVal % 2==0)  
        nValue=-nValue;  
    return nValue;  
}  
  
  
DWORD u(UINT BitCount,BYTE * buf,UINT &nStartBit)  
{  
    DWORD dwRet = 0;  
    for (UINT i=0; i<BitCount; i++)  
    {  
        dwRet <<= 1;  
        if (buf[nStartBit / 8] & (0x80 >> (nStartBit % 8)))  
        {  
            dwRet += 1;  
        }  
        nStartBit++;  
    }  
    return dwRet;  
}  
  
/** 
 * H264的NAL起始码防竞争机制 
 * 
 * @param buf SPS数据内容 
 * 
 * @无返回值 
 */  
void de_emulation_prevention(BYTE* buf,unsigned int* buf_size)  
{  
    int i=0,j=0;  
    BYTE* tmp_ptr=NULL;  
    unsigned int tmp_buf_size=0;  
    int val=0;  
  
    tmp_ptr=buf;  
    tmp_buf_size=*buf_size;  
    for(i=0;i<(tmp_buf_size-2);i++)  
    {  
        //check for 0x000003  
        val=(tmp_ptr[i]^0x00) +(tmp_ptr[i+1]^0x00)+(tmp_ptr[i+2]^0x03);  
        if(val==0)  
        {  
            //kick out 0x03  
            for(j=i+2;j<tmp_buf_size-1;j++)  
                tmp_ptr[j]=tmp_ptr[j+1];  
  
            //and so we should devrease bufsize  
            (*buf_size)--;  
        }  
    }  
}  
  
/** 
 * 解码SPS,获取视频图像宽、高和帧率信息 
 * 
 * @param buf SPS数据内容 
 * @param nLen SPS数据的长度 
 * @param width 图像宽度 
 * @param height 图像高度 
 
 * @成功则返回true , 失败则返回false 
 */  
bool h264_decode_sps(BYTE * buf, unsigned int nLen, int &width, int &height, int &fps)  
{  
    UINT StartBit=0;  
    de_emulation_prevention(buf,&nLen);  
    fps = 0;
  
    int forbidden_zero_bit=u(1,buf,StartBit);  
    int nal_ref_idc=u(2,buf,StartBit);  
    int nal_unit_type=u(5,buf,StartBit);  
    if(nal_unit_type==7)  
    {  
        int profile_idc=u(8,buf,StartBit);  
        int constraint_set0_flag=u(1,buf,StartBit);//(buf[1] & 0x80)>>7;  
        int constraint_set1_flag=u(1,buf,StartBit);//(buf[1] & 0x40)>>6;  
        int constraint_set2_flag=u(1,buf,StartBit);//(buf[1] & 0x20)>>5;  
        int constraint_set3_flag=u(1,buf,StartBit);//(buf[1] & 0x10)>>4;  
        int reserved_zero_4bits=u(4,buf,StartBit);  
        int level_idc=u(8,buf,StartBit);  
  
        int seq_parameter_set_id=Ue(buf,nLen,StartBit);  
  
        if( profile_idc == 100 || profile_idc == 110 ||  
            profile_idc == 122 || profile_idc == 144 )  
        {  
            int chroma_format_idc=Ue(buf,nLen,StartBit);  
            if( chroma_format_idc == 3 )  
                int residual_colour_transform_flag=u(1,buf,StartBit);  
            int bit_depth_luma_minus8=Ue(buf,nLen,StartBit);  
            int bit_depth_chroma_minus8=Ue(buf,nLen,StartBit);  
            int qpprime_y_zero_transform_bypass_flag=u(1,buf,StartBit);  
            int seq_scaling_matrix_present_flag=u(1,buf,StartBit);  
  
            int seq_scaling_list_present_flag[8];  
            if( seq_scaling_matrix_present_flag )  
            {  
                for( int i = 0; i < 8; i++ ) {  
                    seq_scaling_list_present_flag[i]=u(1,buf,StartBit);  
                }  
            }  
        }  
        int log2_max_frame_num_minus4=Ue(buf,nLen,StartBit);  
        int pic_order_cnt_type=Ue(buf,nLen,StartBit);  
        if( pic_order_cnt_type == 0 )  
            int log2_max_pic_order_cnt_lsb_minus4=Ue(buf,nLen,StartBit);  
        else if( pic_order_cnt_type == 1 )  
        {  
            int delta_pic_order_always_zero_flag=u(1,buf,StartBit);  
            int offset_for_non_ref_pic=Se(buf,nLen,StartBit);  
            int offset_for_top_to_bottom_field=Se(buf,nLen,StartBit);  
            int num_ref_frames_in_pic_order_cnt_cycle=Ue(buf,nLen,StartBit);  
  
            int *offset_for_ref_frame=new int[num_ref_frames_in_pic_order_cnt_cycle];  
            for( int i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++ )  
                offset_for_ref_frame[i]=Se(buf,nLen,StartBit);  
            delete [] offset_for_ref_frame;  
        }  
        int num_ref_frames=Ue(buf,nLen,StartBit);  
        int gaps_in_frame_num_value_allowed_flag=u(1,buf,StartBit);  
        int pic_width_in_mbs_minus1=Ue(buf,nLen,StartBit);  
        int pic_height_in_map_units_minus1=Ue(buf,nLen,StartBit);  
  
        width=(pic_width_in_mbs_minus1+1)*16;  
        height=(pic_height_in_map_units_minus1+1)*16;  
  
        int frame_mbs_only_flag=u(1,buf,StartBit);  
        if(!frame_mbs_only_flag)  
            int mb_adaptive_frame_field_flag=u(1,buf,StartBit);  
  
        int direct_8x8_inference_flag=u(1,buf,StartBit);  
        int frame_cropping_flag=u(1,buf,StartBit);  
        if(frame_cropping_flag)  
        {  
            int frame_crop_left_offset=Ue(buf,nLen,StartBit);  
            int frame_crop_right_offset=Ue(buf,nLen,StartBit);  
            int frame_crop_top_offset=Ue(buf,nLen,StartBit);  
            int frame_crop_bottom_offset=Ue(buf,nLen,StartBit);  
        }  
        int vui_parameter_present_flag=u(1,buf,StartBit);  
        if(vui_parameter_present_flag)  
        {  
            int aspect_ratio_info_present_flag=u(1,buf,StartBit);  
            if(aspect_ratio_info_present_flag)  
            {  
                int aspect_ratio_idc=u(8,buf,StartBit);  
                if(aspect_ratio_idc==255)  
                {  
                    int sar_width=u(16,buf,StartBit);  
                    int sar_height=u(16,buf,StartBit);  
                }  
            }  
            int overscan_info_present_flag=u(1,buf,StartBit);  
            if(overscan_info_present_flag)  
                int overscan_appropriate_flagu=u(1,buf,StartBit);  
            int video_signal_type_present_flag=u(1,buf,StartBit);  
            if(video_signal_type_present_flag)  
            {  
                int video_format=u(3,buf,StartBit);  
                int video_full_range_flag=u(1,buf,StartBit);  
                int colour_description_present_flag=u(1,buf,StartBit);  
                if(colour_description_present_flag)  
                {  
                    int colour_primaries=u(8,buf,StartBit);  
                    int transfer_characteristics=u(8,buf,StartBit);  
                    int matrix_coefficients=u(8,buf,StartBit);  
                }  
            }  
            int chroma_loc_info_present_flag=u(1,buf,StartBit);  
            if(chroma_loc_info_present_flag)  
            {  
                int chroma_sample_loc_type_top_field=Ue(buf,nLen,StartBit);  
                int chroma_sample_loc_type_bottom_field=Ue(buf,nLen,StartBit);  
            }  
            int timing_info_present_flag=u(1,buf,StartBit);  
  
            if(timing_info_present_flag)  
            {  
                int num_units_in_tick=u(32,buf,StartBit);  
                int time_scale=u(32,buf,StartBit);  
                fps=time_scale/num_units_in_tick;  
                int fixed_frame_rate_flag=u(1,buf,StartBit);  
                if(fixed_frame_rate_flag)  
                {  
                    fps=fps/2;  
                }  
            }  
        }  
        return true;  
    }  
    else  
        return false;  
}

CSPSReader::CSPSReader()
{

}

CSPSReader::~CSPSReader()
{

}

int
CSPSReader::Do_Read_SPS( bs_t *s, int *width, int *height)
{
	int i_profile_idc;
	int i_level_idc;

	int b_constraint_set0;
	int b_constraint_set1;
	int b_constraint_set2;

	int id;

	int i_log2_max_frame_num;
	int i_poc_type;
	int i_log2_max_poc_lsb;

	int i_num_ref_frames;
	int b_gaps_in_frame_num_value_allowed;
	int i_mb_width;
	int i_mb_height;

	unsigned char *p_data;

	p_data = s->p;

	if( (p_data[0] == 0x00) && (p_data[1] == 0x00) && (p_data[2] == 0x00) && (p_data[3] == 0x01) )
	{
		p_data += 4;
		s->p += 5;
	}

	else
	{
		if((p_data[0]<<8 | p_data[1]) <= 3)
		{
			return -1;
		}
		else 
		{
			//p_data += 2;
			//s->p += 3;
			s->p += 1;
		}
	}

	if((p_data[0] & 0x0F) != 0x07 )
	{
		return -2;
	}

	//s->p += 3;

	i_profile_idc     = _bs_read( s, 8 );
	b_constraint_set0 = _bs_read( s, 1 );
	b_constraint_set1 = _bs_read( s, 1 );
	b_constraint_set2 = _bs_read( s, 1 );

	_bs_skip( s, 5 );    // reserved
	i_level_idc       = _bs_read( s, 8 );


	id = _bs_read_ue( s );			//set_id

	//根据H264白皮书添加by:wuxl   xiao17174@126.com
	if (i_profile_idc == 100 || i_profile_idc == 110 || i_profile_idc == 122 || i_profile_idc == 144)
	{
		int chroma_format_idc;
		int bit_depth_luma_minus8;
		int bit_depth_chroma_minus8;
		int qpprime_y_zero_transform_bypass_flag;
		int seq_scaling_matrix_present_flag;

		chroma_format_idc = _bs_read_ue(s);
		if (chroma_format_idc == 3)
		{
			int residual_colour_transform_flag  = _bs_read( s, 1 );
		}
		bit_depth_luma_minus8	= _bs_read_ue(s);
		bit_depth_chroma_minus8 = _bs_read_ue(s);
		qpprime_y_zero_transform_bypass_flag	= _bs_read( s, 1 );
		seq_scaling_matrix_present_flag			= _bs_read( s, 1 );
		if (seq_scaling_matrix_present_flag)
		{
			int seq_scaling_list_present_flag[8] = {0};
			for(int i = 0; i < 8; i++ ) 
			{ 
				seq_scaling_list_present_flag[i] = _bs_read( s, 1 );
				if( seq_scaling_list_present_flag[i] ) {
					if (i<6) {
						_scaling_list(s,0,16,0);
					} else {
						_scaling_list(s,0,64,0);
					}
				}
			}
		}

	}
	//if( _bs_eof( s ) || id >= 32 )
	//{
	//	// the sps is invalid, no need to corrupt sps_array[0]
	//	return -1;
	//}

	i_log2_max_frame_num = _bs_read_ue( s ) + 4;

	i_poc_type = _bs_read_ue( s );
	//i_poc_type = 1;
	if( i_poc_type == 0 )
	{
		i_log2_max_poc_lsb = _bs_read_ue( s ) + 4;
	}
	else if( i_poc_type == 1 )
	{
		int i;

		int b_delta_pic_order_always_zero;
		int i_offset_for_non_ref_pic;
		int i_offset_for_top_to_bottom_field;
		int i_num_ref_frames_in_poc_cycle;
		int i_offset_for_ref_frame [256];

		b_delta_pic_order_always_zero    = _bs_read( s, 1 );
		i_offset_for_non_ref_pic         = _bs_read_se( s );
		i_offset_for_top_to_bottom_field = _bs_read_se( s );
		i_num_ref_frames_in_poc_cycle    = _bs_read_ue( s );
		if( i_num_ref_frames_in_poc_cycle > 256 )
		{
			// FIXME what to do
			i_num_ref_frames_in_poc_cycle = 256;
		}

		for( i = 0; i < i_num_ref_frames_in_poc_cycle; i++ )
		{
			i_offset_for_ref_frame[i] = _bs_read_se( s );
		}
	}
	else if( i_poc_type > 2 )
	{
		goto error;
	}

	i_num_ref_frames = _bs_read_ue( s );
	b_gaps_in_frame_num_value_allowed = _bs_read( s, 1 );
	i_mb_width  = _bs_read_ue( s ) + 1;
	i_mb_height = _bs_read_ue( s ) + 1;

	*width  = i_mb_width  * 16;
	*height = i_mb_height * 16;

	// add by jackey...
	/*int mb_aff = 0;
	int frame_mbs_only_flag = _bs_read( s, 1 );
	if( !frame_mbs_only_flag )
		mb_aff = _bs_read( s, 1 );
	else
		mb_aff = 0;
	int direct_8x8_inference_flag = _bs_read( s, 1 );
	int frame_cropping_flag = _bs_read( s, 1 );
	if( frame_cropping_flag ) {
		unsigned int crop_left = _bs_read_ue( s );            ///< frame_cropping_rect_left_offset
		unsigned int crop_right = _bs_read_ue( s );           ///< frame_cropping_rect_right_offset
		unsigned int crop_top = _bs_read_ue( s );             ///< frame_cropping_rect_top_offset
		unsigned int crop_bottom = _bs_read_ue( s );          ///< frame_cropping_rect_bottom_offset
	}
	int vui_parameters_present_flag = _bs_read( s, 1 );
	if( vui_parameters_present_flag ) {
		int aspect_ratio_info_present_flag = _bs_read( s, 1 );
		if( aspect_ratio_info_present_flag ) {
			int aspect_ratio_idc = _bs_read( s, 8 );
			if( aspect_ratio_idc == 255 ) {
				int nSarNum = _bs_read( s, 16 );
				int nSarDen = _bs_read( s, 16 );
			}
		}
		int overscan_info_present_flag = _bs_read( s, 1 );
		if( overscan_info_present_flag ) {
			int overscan_appropriate_flag = _bs_read( s, 1 );
		}
		int video_signal_type_present_flag = _bs_read( s, 1 );
		if( video_signal_type_present_flag ) {
			int video_format = _bs_read( s, 3 );
			int full_range = _bs_read( s, 1 );
			int colour_description_present_flag = _bs_read( s, 1 );
			if( colour_description_present_flag ) {
				int color_primaries = _bs_read( s, 8 );
				int color_trc = _bs_read( s, 8 );
				int colorspace = _bs_read( s, 8 );
			}
		}
		int chroma_location_info_present_flag = _bs_read( s, 1 );
		if( chroma_location_info_present_flag ) {
			int chroma_sample_location = _bs_read_ue( s );
			int chroma_sample_location_type_bottom_field = _bs_read_ue( s );
		}
		int timing_info_present_flag = _bs_read( s, 1 );
		if( timing_info_present_flag ) {
			int num_units_in_tick = _bs_read_ue( s );
			int time_scale = _bs_read_ue( s );
			int fps = time_scale / num_units_in_tick;
			int fixed_frame_rate_flag = _bs_read( s, 1 );
			if( fixed_frame_rate_flag ) {
				fps = fps/2;
			}
		}
	}*/

	return id;

error:    
	return -1;
}

int			
CSPSReader::_bs_eof( bs_t *s )
{
	return( s->p >= s->p_end ? 1: 0 );
}

uint32_t	
CSPSReader::_bs_read( bs_t *s, int i_count )
{
	static uint32_t i_mask[33] ={
		0x00,
		0x01,      0x03,      0x07,      0x0f,
		0x1f,      0x3f,      0x7f,      0xff,
		0x1ff,     0x3ff,     0x7ff,     0xfff,
		0x1fff,    0x3fff,    0x7fff,    0xffff,
		0x1ffff,   0x3ffff,   0x7ffff,   0xfffff,
		0x1fffff,  0x3fffff,  0x7fffff,  0xffffff,
		0x1ffffff, 0x3ffffff, 0x7ffffff, 0xfffffff,
		0x1fffffff,0x3fffffff,0x7fffffff,0xffffffff
	};

	int      i_shr;
	uint32_t i_result = 0;

	while( i_count > 0 )
	{
		if( s->p >= s->p_end )
		{
			break;
		}

		if( ( i_shr = s->i_left - i_count ) >= 0 )
		{
			/* more in the buffer than requested */
			i_result |= ( *s->p >> i_shr )&i_mask[i_count];
			s->i_left -= i_count;
			if( s->i_left == 0 )
			{
				s->p++;
				s->i_left = 8;
			}
			return( i_result );
		}
		else
		{
			// less in the buffer than requested
			i_result |= (*s->p&i_mask[s->i_left]) << -i_shr;
			i_count  -= s->i_left;
			s->p++;
			s->i_left = 8;
		}
	}

	return( i_result );
}
void		
CSPSReader::_bs_skip( bs_t *s, int i_count )
{
	s->i_left -= i_count;

	while( s->i_left <= 0 )
	{
		s->p++;
		s->i_left += 8;
	}
}
int		
CSPSReader::_bs_read_ue( bs_t *s )
{
	int i = 0;

	while( _bs_read1( s ) == 0 && s->p < s->p_end && i < 32 )
	{
		i++;
	}

	return( ( 1 << i) - 1 + _bs_read( s, i ) );
}
int		
CSPSReader::_bs_read_se( bs_t *s )
{
	int val = _bs_read_ue( s );

	return val & 0x01 ? (val + 1) / 2 : -(val / 2);
}

uint32_t
CSPSReader::_bs_read1( bs_t *s )
{

	if( s->p < s->p_end )
	{
		unsigned int i_result;

		s->i_left--;
		i_result = ( *s->p >> s->i_left )&0x01;
		if( s->i_left == 0 )
		{
			s->p++;
			s->i_left = 8;
		}
		return i_result;
	}

	return 0;
}
void
CSPSReader::_scaling_list(bs_t *s,int scalingList,int sizeOfScalingList,int useDefaultScalingMatrixFlag ) 
{ 
	int lastScale = 8 ;
	int nextScale = 8 ;
	int	delta_scale;
	int	scalingListEx[64] = {0};	//这是一个伪数组,在白皮书中,这个数组不应该存在于这里.
	for(int j = 0; j < sizeOfScalingList; j++ ) { 
		if( nextScale != 0 ) { 
			delta_scale =   _bs_read_se( s );
			nextScale = ( lastScale + delta_scale + 256 ) % 256 ;
			useDefaultScalingMatrixFlag = ( j  ==  0 && nextScale  ==  0 ) ;
		} 
		scalingListEx[ j ] = ( nextScale  ==  0 ) ? lastScale : nextScale ;
		lastScale = scalingListEx[ j ] ;
	} 
} 