
#pragma once
#include "pch.h"
#define LINE_LEN 16
#include <stdio.h>
#include <tchar.h>
#include "pcap.h"
#include <fstream>
#include "String"
#include <vector>
#include <iostream>
using namespace std;

double last_azimuth;
bool is_first_sweep;
int cir_flag; //完整一圈的标志 初值为不完整 0

#define M_PI  3.14159265358979323846

double min_range;
double max_range;
bool isPointInRange(const double& distance) {
	return (distance >= min_range && distance <= max_range);
}

double cos_azimuth_table[6300];
double sin_azimuth_table[6300];

//点的垂直角度的弧度,C++中三角函数操作的是弧度,而非角度,需要把角度转换为弧度，弧度=角度*Pi/180
static const double scan_altitude[16] = {
	-15 * M_PI / 180,  -13 * M_PI / 180,
	-11 * M_PI / 180,   -9 * M_PI / 180,
	 -7 * M_PI / 180,   -5 * M_PI / 180,
	 -4 * M_PI / 180,   -3 * M_PI / 180,
	 -2 * M_PI / 180,   -1 * M_PI / 180,
	  0 * M_PI / 180,    1 * M_PI / 180,
	  3 * M_PI / 180,    5 * M_PI / 180,
	  7 * M_PI / 180,    9 * M_PI / 180
};

static const double cos_scan_altitude[16] = {
	cos(scan_altitude[0]), cos(scan_altitude[1]),
	cos(scan_altitude[2]), cos(scan_altitude[3]),
	cos(scan_altitude[4]), cos(scan_altitude[5]),
	 cos(scan_altitude[6]),  cos(scan_altitude[7]),
	 cos(scan_altitude[8]),  cos(scan_altitude[9]),
	 cos(scan_altitude[10]),  cos(scan_altitude[11]),
	 cos(scan_altitude[12]),  cos(scan_altitude[13]),
	 cos(scan_altitude[14]),  cos(scan_altitude[15]),
};

static const double sin_scan_altitude[16] = {
	 sin(scan_altitude[0]),  sin(scan_altitude[1]),
	 sin(scan_altitude[2]),  sin(scan_altitude[3]),
	 sin(scan_altitude[4]),  sin(scan_altitude[5]),
	 sin(scan_altitude[6]),  sin(scan_altitude[7]),
	 sin(scan_altitude[8]),  sin(scan_altitude[9]),
	 sin(scan_altitude[10]),  sin(scan_altitude[11]),
	 sin(scan_altitude[12]),  sin(scan_altitude[13]),
	 sin(scan_altitude[14]),  sin(scan_altitude[15]),
};

struct Coord	//一个坐标点
{
	bool isValid;		//是否为有效点
	bool isSmoothValid;	//该点左右numSmoothPoints个点是否都有效，即是否可以进行平滑度计算
	float distance;		//距离
	float xyDistance;	//激光雷达坐标xy平面距离
	float xOrigin;		//激光雷达坐标系x值
	float yOrigin;		//激光雷达坐标系y值
	float zOrigin;		//激光雷达坐标系z值
	float x;			//图像坐标系x值
	float y;			//图像坐标系y值
	float z;			//图像坐标系z值
	int intensity;	//反射强度
	bool isSmooth;		//是否平滑
	bool operator<(const Coord p) const
	{
		if (z < p.z)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
};

int rotpos;  //每旋转一个角度加1，即每收集到一个firing加1，直到该圈结束，最大值为1799
Coord** CircleCoords;		//旋转一圈得到的所有坐标点，已经排序好的  CircleCoords[线序号][点序号]
int CircleCoords_num[16];

typedef unsigned char hrl_uint8_t;		//一个字节
typedef unsigned short int hrl_uint16_t;//两个字节
typedef unsigned long hrl_uint32_t;		//四个字节


#define DEG_TO_RAD 0.017453292
static const double DISTANCE_RESOLUTION = 0.004; /*回波距离单位为 4.0mm */
static const double  DSR_TOFFSET = 2.304;   // [µs]
static const double  FIRING_TOFFSET = 55.296;  // [µs]


const u_char *pkt_data;
const unsigned char *pData;		//读取数据的指针
int num_packet;//记录每一圈中有多少packet


static const int SCANS_PER_FIRING = 16;
static const int FIRINGS_PER_BLOCK = 2;

static const int SIZE_BLOCK = 100;
static const int RAW_SCAN_SIZE = 3;
static const int SCANS_PER_BLOCK = 32;
static const int BLOCKS_PER_PACKET = 12;
static const int BLOCK_DATA_SIZE =
(SCANS_PER_BLOCK * RAW_SCAN_SIZE);
static const int FIRINGS_PER_PACKET =
FIRINGS_PER_BLOCK * BLOCKS_PER_PACKET;




double rawAzimuthToDouble(const uint16_t& raw_azimuth) {
	// According to the user manual,
	// azimuth = raw_azimuth / 100.0;
	/*int raw_azimuth2=raw_azimuth+0;
	if (raw_azimuth2>36000)
	{
		raw_azimuth2=raw_azimuth2-36000;
	}*/
	return static_cast<double>(raw_azimuth) / 100.0 * DEG_TO_RAD;
}

union TwoBytes {
	hrl_uint16_t distance;
	hrl_uint8_t  bytes[2];
};

struct RawBlock {
	hrl_uint16_t header;        ///< UPPER_BANK or LOWER_BANK
	hrl_uint16_t rotation;      ///< 0-35999, divide by 100 to get degrees
	hrl_uint8_t  data[BLOCK_DATA_SIZE];
};

struct RawPacket {
	RawBlock blocks[BLOCKS_PER_PACKET];
	hrl_uint32_t time_stamp;
	hrl_uint8_t factory[2];
	//uint16_t revolution;
	//uint8_t status[PACKET_STATUS_SIZE];
};

struct Firing {
	// Azimuth associated with the first shot within this firing.
	double firing_azimuth;
	double azimuth[SCANS_PER_FIRING];
	double distance[SCANS_PER_FIRING];
	double intensity[SCANS_PER_FIRING];

};

Firing firings[FIRINGS_PER_PACKET];//定义存储firing的数据空间

void decodePacket( const RawPacket* packet ) {



	// 计算每个group的第一个点的方位角
	for (size_t fir_idx = 0; fir_idx < FIRINGS_PER_PACKET; fir_idx += 2) {
		size_t blk_idx = fir_idx / 2;
		firings[fir_idx].firing_azimuth = 
			packet->blocks[blk_idx].rotation;
		//rotation_record<<packet->blocks[blk_idx].rotation<<endl;
	}

	//计算每组数据的第一个点的方位角,fir_idx为packet中16线数据的组数
	for (size_t fir_idx = 1; fir_idx < FIRINGS_PER_PACKET; fir_idx += 2) {
		size_t lfir_idx = fir_idx - 1;
		size_t rfir_idx = fir_idx + 1;

		if (fir_idx == FIRINGS_PER_PACKET - 1) {
			lfir_idx = fir_idx - 3;
			rfir_idx = fir_idx - 1;
		}

		double azimuth_diff = firings[rfir_idx].firing_azimuth -
			firings[lfir_idx].firing_azimuth;
		azimuth_diff = azimuth_diff < 0 ? azimuth_diff + 2 * M_PI : azimuth_diff;

		firings[fir_idx].firing_azimuth =
			firings[fir_idx - 1].firing_azimuth + azimuth_diff / 2.0;
		firings[fir_idx].firing_azimuth =
			firings[fir_idx].firing_azimuth > 2 * M_PI ?
			firings[fir_idx].firing_azimuth - 2 * M_PI : firings[fir_idx].firing_azimuth;

		/*printf("0x%x\n",firings[fir_idx].firing_azimuth);*/
	}

	// 计算每个点的回波距离，方位角，回波强度
	for (size_t blk_idx = 0; blk_idx < BLOCKS_PER_PACKET; ++blk_idx) {
		const RawBlock& raw_block = packet->blocks[blk_idx];

		for (size_t blk_fir_idx = 0; blk_fir_idx < FIRINGS_PER_BLOCK; ++blk_fir_idx) {
			size_t fir_idx = blk_idx * FIRINGS_PER_BLOCK + blk_fir_idx;

			double azimuth_diff = 0.0;
			if (fir_idx < FIRINGS_PER_PACKET - 1)
				azimuth_diff = firings[fir_idx + 1].firing_azimuth -
				firings[fir_idx].firing_azimuth;
			else
				azimuth_diff = firings[fir_idx].firing_azimuth -
				firings[fir_idx - 1].firing_azimuth;
			if (azimuth_diff < 0)
			{
				azimuth_diff += 2 * M_PI;
			}

			for (size_t scan_fir_idx = 0; scan_fir_idx < SCANS_PER_FIRING; ++scan_fir_idx) {
				//byte_idx是point第一个字节的序号，一个point3个字节
				size_t byte_idx = RAW_SCAN_SIZE * (
					SCANS_PER_FIRING*blk_fir_idx + scan_fir_idx);

				// Azimuth
				firings[fir_idx].azimuth[scan_fir_idx] = (firings[fir_idx].firing_azimuth +
					(double)(scan_fir_idx/15.0) * (double)azimuth_diff)*0.01;

				// Distance
				TwoBytes raw_distance;
				raw_distance.bytes[0] = raw_block.data[byte_idx];
				raw_distance.bytes[1] = raw_block.data[byte_idx + 1];
				firings[fir_idx].distance[scan_fir_idx] = static_cast<double>(
					raw_distance.distance) * DISTANCE_RESOLUTION;

				// Intensity
				firings[fir_idx].intensity[scan_fir_idx] = static_cast<double>(
					raw_block.data[byte_idx + 2]);

				cout <<"角度(°):"<< firings[fir_idx].azimuth[scan_fir_idx]<<"   回波距离(m)："<< firings[fir_idx].distance[scan_fir_idx]
					<< "   回波强度："<< firings[fir_idx].intensity[scan_fir_idx]<< endl;
	
			}
			printf("\n\n");
		}
	}
	return;

}

int decodeCircle(const u_char* pkt_data) {

	const RawPacket* raw_packet;
	pData = pkt_data + 42;
	raw_packet = (const RawPacket*)(pData);
	decodePacket(raw_packet);
	num_packet++;

	//size_t new_sweep_start = 0;
	//do {
	//	if (firings[new_sweep_start].firing_azimuth < last_azimuth/*&&(last_azimuth-2*PI)<0.01*/)
	//	{
	//		if (1 != is_first_sweep)
	//		{
	//			cir_flag = 1;//已经找到完整的一圈
	//		}
	//		//packet_num_record<<num_packet<<"\t"<<last_azimuth<<"\t"<<firings[new_sweep_start].firing_azimuth<<endl;
	//		last_azimuth = firings[new_sweep_start].firing_azimuth;
	//		//firing_angle<<"\n"<<"\n";


	//		num_packet = 0;
	//		break;//如果找到新的一圈，新角度值小于上一个角度
	//	}
	//	else
	//	{
	//		cir_flag = 0;
	//		last_azimuth = firings[new_sweep_start].firing_azimuth;
	//		++new_sweep_start;
	//	}
	//} while (new_sweep_start < FIRINGS_PER_PACKET);//如果没有新的一圈数据new_sweep_start等于FIRINGS_PER_PACKET  

	//size_t start_fir_idx = 0;
	//size_t end_fir_idx = new_sweep_start;
	//if (is_first_sweep &&
	//	new_sweep_start == FIRINGS_PER_PACKET) {
	//	// The first sweep has not ended yet.
	//	return 0;  //将第一圈数据丢弃
	//}
	//else
	//{
	//	if (is_first_sweep)
	//	{  //如果接收到了第二圈数据，赋值给start_fir_idx = new_sweep_start
	//		is_first_sweep = false;
	//		start_fir_idx = new_sweep_start;
	//		end_fir_idx = FIRINGS_PER_PACKET;
	//		/*  sweep_start_time = msg->stamp.toSec() +
	//			  FIRING_TOFFSET * (end_fir_idx-start_fir_idx) * 1e-6;*/
	//	}
	//}

	//for (size_t fir_idx = start_fir_idx; fir_idx < end_fir_idx; ++fir_idx)
	//{
	//	for (size_t scan_idx = 0; scan_idx < SCANS_PER_FIRING; ++scan_idx) {
	//		// Check if the point is valid.
	//		if (!isPointInRange(firings[fir_idx].distance[scan_idx])) continue;

	//		// Convert the point to xyz coordinate

	//		size_t table_idx = floor(firings[fir_idx].azimuth[scan_idx] * 1000.0 + 0.5);
	//		//idx_record << table_idx << "\t"<<firings[fir_idx].azimuth[scan_idx]<<endl;
	//		double cos_azimuth = cos_azimuth_table[table_idx];
	//		double sin_azimuth = sin_azimuth_table[table_idx];

	//		double x = firings[fir_idx].distance[scan_idx] *
	//			cos_scan_altitude[scan_idx] * sin_azimuth;
	//		double y = firings[fir_idx].distance[scan_idx] *
	//			cos_scan_altitude[scan_idx] * cos_azimuth;
	//		double z = firings[fir_idx].distance[scan_idx] *
	//			sin_scan_altitude[scan_idx];
	//		double intensity = firings[fir_idx].intensity[scan_idx];
	//		/*if ((0==rotpos||1==rotpos)&&scan_idx==14)
	//		{
	//			int ss=0;
	//			ss=1;
	//		}*/
	//		double x_coord = x;
	//		double y_coord = y;
	//		double z_coord = z;
	//		double intensity_coord = intensity;

	//		/*Rotation(x_coord, y_coord, z_coord);*/

	//		// Compute the time of the point
	//	   /* double time = packet_start_time +
	//			FIRING_TOFFSET*fir_idx + DSR_TOFFSET*scan_idx;*/

	//			// Remap the index of the scan
	//		int remapped_scan_idx = scan_idx % 2 == 0 ? scan_idx / 2 : scan_idx / 2 + 8;
	//		CircleCoords[remapped_scan_idx][rotpos].x = x_coord;
	//		CircleCoords[remapped_scan_idx][rotpos].y = y_coord;
	//		CircleCoords[remapped_scan_idx][rotpos].z = z_coord;
	//		CircleCoords[remapped_scan_idx][rotpos].intensity = intensity_coord;
	//		CircleCoords_num[remapped_scan_idx] = rotpos;
	//		//ASSERT(rotpos<=1800);
	//	}
	//	rotpos++;
	//}

	return 0;
}

int _tmain(int argc, _TCHAR* argv[]) {
	pcap_if_t *alldevs, *d;
	pcap_t *fp;  //在线&离线数据指针
	u_int inum, i = 0;
	char errbuf[PCAP_ERRBUF_SIZE];
	int res;
	struct pcap_pkthdr *header;

	if (argc < 3) {
		printf("选择设备：\n");
		if (pcap_findalldevs(&alldevs, errbuf) == -1) {
			printf("error: \n", errbuf);
			exit(1);
		}

		//打印设备列表
		for (d = alldevs; d; d = d->next) {
			printf("%d. %s\n", ++i, d->name);
			if (d->description)
				printf("(%s)\n", d->description);
			else
			{
				printf("无设备详情");
			}
		}

		if (i == 0) {
			printf("winpcap未安装");
		}
		printf("请输入设备序号(1-%d):", i);
		scanf_s("%d", &inum);

		if (inum<1 || inum>i) {
			printf("输入的序号错误");
			pcap_freealldevs(alldevs);
			return -1;
		}


		//选择适配器
		for (d = alldevs, i = 0; i < inum - 1; d = d->next, i++);
		//打开适配器
		if ((fp = pcap_open_live(d->name, 65536, 1, 1000, errbuf)) == NULL) {
			printf("打开适配器错误");
			return -1;
		}

		//读包
		while ((res = pcap_next_ex(fp, &header, &pkt_data)) >= 0) {		//pkt_data中存储着数据，pcap_next_ex()每次捕获一个数据包，成功捕获则返回1
			if (res == 0)
				continue;

			/////////////////////////////////////////////////////////////////////////////////////将数据显示在控制台
			//printf("%1d:%1d(%1d)\n", header->ts.tv_sec, header->ts.tv_usec, header->len);

			////打印包
			//for (i = 1; (i < header->caplen + 1); i++) {
			//	printf("%.2x", pkt_data[i - 1]);
			//	if ((i%LINE_LEN) == 0)
			//		printf("\n");
			//}
			//printf("\n\n");
			/////////////////////////////////////////////////////////////////////////////////////

			decodeCircle(pkt_data);



		}
		if (res == -1) {
			printf("读文件错误:&s\n", pcap_geterr(fp));
			return -1;
		}

		pcap_close(fp);
	}
}