/*  
    EQ2Emulator:  Everquest II Server Emulator
    Copyright (C) 2007  EQ2EMulator Development Team (http://www.eq2emulator.net)

    This file is part of EQ2Emulator.

    EQ2Emulator is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    EQ2Emulator is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with EQ2Emulator.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifdef WIN32
#include <WinSock2.h>
	#include <windows.h>
#endif
#include "debug.h"
#include <string>
#include <iomanip>
#include <iostream>
#include <vector>
#include <time.h>
#include <sys/types.h>
#ifdef WIN32
	#include <time.h>
#else
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <sys/time.h>
	#include <sys/socket.h>
	#include <netdb.h>
	#include <fcntl.h>
	#include <arpa/inet.h>
#endif
#include "EQPacket.h"
#include "EQStream.h"
#include "EQStreamFactory.h"
#include "misc.h"
#include "Mutex.h"
#include "op_codes.h"
#include "CRC16.h"
#include "packet_dump.h"
#ifdef LOGIN
	#include "../LoginServer/login_structs.h"
#endif
#include "EQ2_Common_Structs.h"
#include "Log.h"

uint16 EQStream::MaxWindowSize=2048;

void EQStream::init() {
	timeout_delays = 0;
	active_users = 0;
	Session=0;
	Key=0;
	MaxLen=0;
	NextInSeq=0;
	NextOutSeq=0;
	CombinedAppPacket=NULL;
	MaxAckReceived=-1;
	NextAckToSend=-1;
	LastAckSent=-1;
	LastSeqSent=-1;
	MaxSends=5;
	LastPacket=Timer::GetCurrentTime2();
	oversize_buffer=NULL;
	oversize_length=0;
	oversize_offset=0;
	Factory = NULL;
	RateThreshold=RATEBASE/250;
	DecayRate=DECAYBASE/250;
	BytesWritten=0;
	crypto->setRC4Key(0);
}

EQStream::EQStream(sockaddr_in addr){ 
	crypto = new Crypto();
	resend_que_timer = new Timer(1000);
	combine_timer = new Timer(250); //250 milliseconds
	combine_timer->Start();
	resend_que_timer->Start();
	init(); 
	remote_ip=addr.sin_addr.s_addr; 
	remote_port=addr.sin_port; 
	State=CLOSED; 
	StreamType=UnknownStream; 
	compressed=true; 
	encoded=false; 
	app_opcode_size=2; 
	#ifdef WIN32
        ZeroMemory(&stream, sizeof(z_stream));
    #else
		bzero(&stream, sizeof(z_stream));
    #endif
	stream.zalloc = (alloc_func)0;
	stream.zfree = (free_func)0;
	stream.opaque = (voidpf)0;
	deflateInit2(&stream, 9, Z_DEFLATED, 13, 9, Z_DEFAULT_STRATEGY);
	//deflateInit(&stream, 5);
	compressed_offset = 0;
	client_version = 0;
	received_packets = 0;
	sent_packets = 0;
}

EQProtocolPacket* EQStream::ProcessEncryptedData(uchar* data, int32 size, int16 opcode){
	//cout << "B4:\n";
	//DumpPacket(data, size);
	/*if(size >= 2 && data[0] == 0 && data[1] == 0){
		cout << "Attempting to fix packet!\n";
		//Have to fix bad packet from client or it will screw up encryption :P
		size--;
		data++;
	}*/
	crypto->RC4Decrypt(data,size);
	int8 offset = 0;
	if(data[0] == 0xFF && size > 2){
			offset = 3;
			memcpy(&opcode, data+sizeof(int8), sizeof(int16));
	}
	else{
		offset = 1;
		memcpy(&opcode, data, sizeof(int8));
	}
	//cout << "After:\n";
	//DumpPacket(data, size);
	return new EQProtocolPacket(opcode, data+offset, size - offset);
}

EQProtocolPacket* EQStream::ProcessEncryptedPacket(EQProtocolPacket *p){
	EQProtocolPacket* ret = NULL;
	if(p->opcode == OP_Packet && p->size > 2)
		ret = ProcessEncryptedData(p->pBuffer+2, p->size-2, p->opcode);
	else
		ret = ProcessEncryptedData(p->pBuffer, p->size, p->opcode);
	return ret;
}

bool EQStream::HandleEmbeddedPacket(EQProtocolPacket *p, int16 offset, int16 length){
	if(p && p->size >= ((uint32)(offset+2))){		
		if(p->pBuffer[offset] == 0 && p->pBuffer[offset+1] == 0x19){
			if(length == 0)
				length = p->size-2-offset;
			else
				length-=2;
#ifdef LE_DEBUG
			LogWrite(PACKET__DEBUG, 0, "Packet", "Creating OP_AppCombined Packet!");
#endif
			EQProtocolPacket *subp=new EQProtocolPacket(OP_AppCombined, p->pBuffer+2+offset, length);
			subp->copyInfo(p);
			ProcessPacket(subp);
			safe_delete(subp);
			return true;
		}
		else if(p->pBuffer[offset] == 0 && p->pBuffer[offset+1] == 0){
			if(length == 0)
				length = p->size-1-offset;
			else
				length--;
#ifdef LE_DEBUG
			LogWrite(PACKET__DEBUG, 0, "Packet", "Creating Opcode 0 Packet!");
			DumpPacket(p->pBuffer+1+offset, length);
#endif
			EQProtocolPacket* newpacket = ProcessEncryptedData(p->pBuffer+1+offset, length, OP_Packet);			
			if(newpacket){
#ifdef LE_DEBUG
				LogWrite(PACKET__DEBUG, 0, "Packet", "Result: ");
				DumpPacket(newpacket);
#endif
				EQApplicationPacket *ap = newpacket->MakeApplicationPacket(2);
				InboundQueuePush(ap);
				safe_delete(newpacket);
			}
			else
				LogWrite(PACKET__ERROR, 0, "Packet", "No Packet!");
			return true;
		}
	}
	return false;
}

void EQStream::ProcessPacket(EQProtocolPacket *p)
{
	uint32 processed=0,subpacket_length=0;
	if (p) {

		if (p->opcode!=OP_SessionRequest && p->opcode!=OP_SessionResponse && !Session) {
#ifdef EQN_DEBUG
			LogWrite(PACKET__ERROR, 0, "Packet", "*** Session not initialized, packet ignored ");
			//p->DumpRaw();
#endif
			return;
		}
 		//cout << "Received " << (int)p->opcode << ":\n";
		//DumpPacket(p->pBuffer, p->size);
		switch (p->opcode) {
			case OP_Combined: {
				processed=0;
				int8 offset = 0;
				int count = 0;
#ifdef LE_DEBUG
				LogWrite(PACKET__DEBUG, 0, "Packet", "OP_Combined: ");
				DumpPacket(p);
#endif
				while(processed<p->size) {
					if ((subpacket_length=(unsigned char)*(p->pBuffer+processed))==0xff) {
						subpacket_length=ntohs(*(uint16 *)(p->pBuffer+processed+1));
						offset = 3;
					}
					else
						offset = 1;
					count++;
#ifdef LE_DEBUG
					LogWrite(PACKET__DEBUG, 0, "Packet", "OP_Combined Packet %i (%u) (%u): ", count, subpacket_length, processed);
#endif
					EQProtocolPacket *subp=new EQProtocolPacket(p->pBuffer+processed+offset,subpacket_length);
					subp->copyInfo(p);
#ifdef LE_DEBUG
					LogWrite(PACKET__DEBUG, 0, "Packet", "Opcode %i:", subp->opcode);
					DumpPacket(subp);
#endif
					ProcessPacket(subp);
#ifdef LE_DEBUG
					DumpPacket(subp);
#endif
					delete subp;
					processed+=subpacket_length+offset;
				}
				break;
			}
			case OP_AppCombined: {
				processed=0;
				EQProtocolPacket* newpacket = 0;
				int8 offset = 0;
#ifdef LE_DEBUG
				LogWrite(PACKET__DEBUG, 0, "Packet", "OP_AppCombined: ");
				DumpPacket(p);
#endif
				int count = 0;
				while(processed<p->size) {
					count++;
					if ((subpacket_length=(unsigned char)*(p->pBuffer+processed))==0xff) {
						subpacket_length=ntohs(*(uint16 *)(p->pBuffer+processed+1));
						offset = 3;
					} else
						offset = 1;
					if(crypto->getRC4Key()==0 && p->size >= 70){
						processRSAKey(p);
					}
					else if(crypto->isEncrypted()){
#ifdef LE_DEBUG
						LogWrite(PACKET__DEBUG, 0, "Packet", "OP_AppCombined Packet %i (%u) (%u): ", count, subpacket_length, processed);
						DumpPacket(p->pBuffer+processed+offset, subpacket_length);
#endif
						if(!HandleEmbeddedPacket(p, processed + offset, subpacket_length)){
#ifdef LE_DEBUG
							LogWrite(PACKET__DEBUG, 0, "Packet", "OP_AppCombined Here:");
#endif
							newpacket = ProcessEncryptedData(p->pBuffer+processed + offset, subpacket_length, OP_AppCombined);						
							if(newpacket){
#ifdef LE_DEBUG
								LogWrite(PACKET__DEBUG, 0, "Packet", "Opcode %i:", newpacket->opcode);
								DumpPacket(newpacket);
#endif
								EQApplicationPacket* ap = newpacket->MakeApplicationPacket(2);
#ifdef LE_DEBUG
								LogWrite(PACKET__DEBUG, 0, "Packet", "OP_AppCombined Here2:");
								DumpPacket(ap);
#endif
								InboundQueuePush(ap);
								safe_delete(newpacket);
							}						
						}
					}
					processed+=subpacket_length+offset;
				}
			}
			break;
			case OP_Packet: {
				uint16 seq=ntohs(*(uint16 *)(p->pBuffer));
				sint8 check=CompareSequence(NextInSeq,seq);
				if (check>0) {
#ifdef EQN_DEBUG
					LogWrite(PACKET__DEBUG, 1, "Packet", "*** Future packet: Expecting Seq=%i, but got Seq=%i", NextInSeq, seq);
					LogWrite(PACKET__DEBUG, 1, "Packet", "[Start]");
					//p->DumpRawHeader(seq);
					LogWrite(PACKET__DEBUG, 1, "Packet", "[End]");
#endif				
					OutOfOrderpackets[seq] = p->Copy();
					SendOutOfOrderAck(seq);
				} else if (check<0) {
#ifdef EQN_DEBUG
					LogWrite(PACKET__DEBUG, 1, "Packet", "*** Duplicate packet: Expecting Seq=%i, but got Seq=%i", NextInSeq, seq);
					LogWrite(PACKET__DEBUG, 1, "Packet", "[Start]");
					//p->DumpRawHeader(seq);
					LogWrite(PACKET__DEBUG, 1, "Packet", "[End]");
#endif
					OutOfOrderpackets[seq] = p->Copy();
					SendOutOfOrderAck(seq);
				} else {
					SetNextAckToSend(seq);
					NextInSeq++;
					if(HandleEmbeddedPacket(p))
						break;
					if(crypto->getRC4Key()==0 && p && p->size >= 70){
						processRSAKey(p);
					}
					else if(crypto->isEncrypted() && p){
						EQProtocolPacket* newpacket = ProcessEncryptedPacket(p);
						if(newpacket){
							EQApplicationPacket *ap = newpacket->MakeApplicationPacket(2);
							InboundQueuePush(ap);
							safe_delete(newpacket);
						}
					}
				}
			}
			break;
			case OP_Fragment: {
				uint16 seq=ntohs(*(uint16 *)(p->pBuffer));
				sint8 check=CompareSequence(NextInSeq,seq);
				if (check>0) {
#ifdef EQN_DEBUG
					LogWrite(PACKET__DEBUG, 1, "Packet", "*** Future packet2: Expecting Seq=%i, but got Seq=%i", NextInSeq, seq);
					LogWrite(PACKET__DEBUG, 1, "Packet", "[Start]");
					//p->DumpRawHeader(seq);
					LogWrite(PACKET__DEBUG, 1, "Packet", "[End]");
#endif
					OutOfOrderpackets[seq] = p->Copy();
					SendOutOfOrderAck(seq);
				} else if (check<0) {
#ifdef EQN_DEBUG
					LogWrite(PACKET__DEBUG, 1, "Packet", "*** Duplicate packet2: Expecting Seq=%i, but got Seq=%i", NextInSeq, seq);
					LogWrite(PACKET__DEBUG, 1, "Packet", "[Start]");
					//p->DumpRawHeader(seq);
					LogWrite(PACKET__DEBUG, 1, "Packet", "[End]");
#endif
					OutOfOrderpackets[seq] = p->Copy();
					SendOutOfOrderAck(seq);
				} else {
					SetNextAckToSend(seq);
					NextInSeq++;
					if (oversize_buffer) {
						memcpy(oversize_buffer+oversize_offset,p->pBuffer+2,p->size-2);
						oversize_offset+=p->size-2;
						//cout << "Oversized is " << oversize_offset << "/" << oversize_length << " (" << (p->size-2) << ") Seq=" << seq << endl;
						if (oversize_offset==oversize_length) {
							if (*(p->pBuffer+2)==0x00 && *(p->pBuffer+3)==0x19) {
								EQProtocolPacket *subp=new EQProtocolPacket(oversize_buffer,oversize_offset);
								subp->copyInfo(p);
								ProcessPacket(subp);
								delete subp;
							} else {
								
								if(crypto->isEncrypted() && p && p->size > 2){
									EQProtocolPacket* p2 = ProcessEncryptedData(oversize_buffer, oversize_offset, p->opcode);
									EQApplicationPacket *ap = p2->MakeApplicationPacket(2);
									ap->copyInfo(p);
									InboundQueuePush(ap);
									safe_delete(p2);
								}
							}
							delete[] oversize_buffer;
							oversize_buffer=NULL;
							oversize_offset=0;
						}
					} else if (!oversize_buffer) {
						oversize_length=ntohl(*(uint32 *)(p->pBuffer+2));
						oversize_buffer=new unsigned char[oversize_length];
						memcpy(oversize_buffer,p->pBuffer+6,p->size-6);
						oversize_offset=p->size-6;
						//cout << "Oversized is " << oversize_offset << "/" << oversize_length << " (" << (p->size-6) << ") Seq=" << seq << endl;
					}
				}
			}
			break;
			case OP_KeepAlive: {
#ifndef COLLECTOR
				NonSequencedPush(new EQProtocolPacket(p->opcode,p->pBuffer,p->size));
#endif
			}
			break;
			case OP_Ack: {
				uint16 seq=ntohs(*(uint16 *)(p->pBuffer));
				SetMaxAckReceived(seq);
			}
			break;
			case OP_SessionRequest: {
				if(GetState() == ESTABLISHED){//reset state
					SetState(CLOSED);
					break;
				}
				init();
				SessionRequest *Request=(SessionRequest *)p->pBuffer;
				Session=ntohl(Request->Session);
				SetMaxLen(ntohl(Request->MaxLength));
#ifndef COLLECTOR
				NextInSeq=0;
				Key=0x33624702;
				SendSessionResponse();
#endif
				SetState(ESTABLISHED);
			}
			break;
			case OP_SessionResponse: {
				init();
				SessionResponse *Response=(SessionResponse *)p->pBuffer;
				SetMaxLen(ntohl(Response->MaxLength));
				Key=ntohl(Response->Key);
				NextInSeq=0;
				SetState(ESTABLISHED);
				if (!Session)
					Session=ntohl(Response->Session);
				compressed=(Response->Format&FLAG_COMPRESSED);
				encoded=(Response->Format&FLAG_ENCODED);

				// Kinda kludgy, but trie for now
				if (compressed) {
					if (remote_port==9000 || (remote_port==0 && p->src_port==9000))
						SetStreamType(WorldStream);
					else
						SetStreamType(ZoneStream);
				} else if (encoded)
					SetStreamType(ChatOrMailStream);
				else
					SetStreamType(LoginStream);
			}
			break;
			case OP_SessionDisconnect: {
				//NextInSeq=0;
				SendDisconnect();
				//SetState(CLOSED);
			}
			break;
			case OP_OutOfOrderAck: {
#ifndef COLLECTOR
				uint16 seq=ntohs(*(uint16 *)(p->pBuffer));
				if (CompareSequence(GetMaxAckReceived(),seq)>0 && CompareSequence(NextOutSeq,seq) < 0) {
					SetLastSeqSent(GetMaxAckReceived());
				}
#endif
			}
			break;
			case OP_ServerKeyRequest:{
				ClientSessionStats* Stats = (ClientSessionStats*)p->pBuffer;
				int16 request_id = Stats->RequestID;
				AdjustRates(ntohl(Stats->average_delta));
				ServerSessionStats* stats=(ServerSessionStats*)p->pBuffer;
                memset(stats, 0, sizeof(ServerSessionStats));
				stats->RequestID = request_id;
				stats->current_time = ntohl(Timer::GetCurrentTime2());
				stats->sent_packets = ntohl(sent_packets);
				stats->sent_packets2 = ntohl(sent_packets);
				stats->received_packets = ntohl(received_packets);
				stats->received_packets2 = ntohl(received_packets);
				NonSequencedPush(new EQProtocolPacket(OP_SessionStatResponse,p->pBuffer,p->size));
				if(!crypto->isEncrypted())
					SendKeyRequest();
			}
			break;
			case OP_SessionStatResponse: {
			}
			break;
			case OP_OutOfSession: {
			}
			break;
			default:
				EQApplicationPacket *ap = p->MakeApplicationPacket(app_opcode_size);
				InboundQueuePush(ap);
				break;
		}
		if (OutOfOrderpackets.find(NextInSeq) != OutOfOrderpackets.end()){
			p = OutOfOrderpackets[NextInSeq];
			// Remove te packet from the list
			OutOfOrderpackets.erase(NextInSeq);
			if (p){

#ifdef EQN_DEBUG
				LogWrite(PACKET__DEBUG, 1, "Packet", "*** Processing Future packet: Seq=%i", NextInSeq);
				LogWrite(PACKET__DEBUG, 1, "Packet", "[Start]");
				//p->DumpRawHeader(NextInSeq);
				LogWrite(PACKET__DEBUG, 1, "Packet", "[End]");
#endif
				ProcessPacket(p);
				safe_delete(p);
			}
		}
	}
}
int8 EQStream::EQ2_Compress(EQ2Packet* app, int8 offset){

#ifdef LE_DEBUG
	LogWrite(PACKET__DEBUG, 0, "Packet", "Before Compress in %s, line %i:", __FUNCTION__, __LINE__);
	DumpPacket(app);
#endif

	uchar* pDataPtr = app->pBuffer + offset;
	uchar* deflate_buff = new uchar[app->size];
	MCompressData.lock();
	stream.next_in  = pDataPtr;
	stream.avail_in = app->size - offset;
	stream.next_out = deflate_buff;
	stream.avail_out = app->size;

	deflate(&stream, Z_SYNC_FLUSH);
	int32 newsize = app->size - stream.avail_out;
	safe_delete_array(app->pBuffer);
	app->size = newsize + offset;
	app->pBuffer = new uchar[app->size];
	app->pBuffer[(offset-1)] = 1;
	memcpy(app->pBuffer + offset, deflate_buff, newsize);
	MCompressData.unlock();
	safe_delete_array(deflate_buff);

#ifdef LE_DEBUG
	LogWrite(PACKET__DEBUG, 0, "Packet", "After Compress in %s, line %i:", __FUNCTION__, __LINE__);
	DumpPacket(app);
#endif

	return offset - 1;
}

int16 EQStream::processRSAKey(EQProtocolPacket *p){
	/*int16 limit = 0;
	int8 offset = 13;
	int8 offset2 = 0;
	if(p->pBuffer[2] == 0)
		limit = p->pBuffer[9];
	else{
		limit = p->pBuffer[5];
		offset2 = 5;
		offset-=1;
	}
	crypto->setRC4Key(Crypto::RSADecrypt(p->pBuffer + offset + (limit-8), 8));
	return (limit + offset +1) - offset2;*/
	if(p->pBuffer[0] == 0)
		crypto->setRC4Key(Crypto::RSADecrypt(p->pBuffer + 62, 8));
	else
		crypto->setRC4Key(Crypto::RSADecrypt(p->pBuffer + 61, 8));
	return 0;
}

void EQStream::SendKeyRequest(){
	int32 crypto_key_size = 60;
	int16 size = sizeof(KeyGen_Struct) + sizeof(KeyGen_End_Struct) + crypto_key_size;
	EQ2Packet *outapp=new EQ2Packet(OP_WSLoginRequestMsg,NULL,size);
	memcpy(&outapp->pBuffer[0], &crypto_key_size, sizeof(int32));
	memset(&outapp->pBuffer[4], 0xFF, crypto_key_size);
	memset(&outapp->pBuffer[size-5], 1, 1);
	memset(&outapp->pBuffer[size-1], 1, 1);
	EQ2QueuePacket(outapp);
}

void EQStream::EncryptPacket(EQ2Packet* app, int8 compress_offset, int8 offset){
	if(app->size>2 && crypto->isEncrypted()){
		app->packet_encrypted = true;
		uchar* crypt_buff = app->pBuffer;
		if(app->eq2_compressed)
			crypto->RC4Encrypt(crypt_buff + compress_offset, app->size - compress_offset);
		else
			crypto->RC4Encrypt(crypt_buff + 2 + offset, app->size - 2 - offset);
	}
}

void EQStream::EQ2QueuePacket(EQ2Packet* app, bool attempted_combine){
	if(CheckActive()){
		if(app->size < 600 && !attempted_combine){
			MCombineQueueLock.lock();
			combine_queue.push_back(app);
			MCombineQueueLock.unlock();
		}
		else{
			PreparePacket(app);
#ifdef LE_DEBUG
			LogWrite(PACKET__DEBUG, 0, "Packet", "After B in %s, line %i:", __FUNCTION__, __LINE__);
			DumpPacket(app);
#endif
			SendPacket(app);
		}
	}
}

void EQStream::UnPreparePacket(EQ2Packet* app){
	if(app->pBuffer[2] == 0 && app->pBuffer[3] == 19){
		uchar* new_buffer = new uchar[app->size-3];
		memcpy(new_buffer+2, app->pBuffer+5, app->size-3);
		delete[] app->pBuffer;
		app->size-=3;
		app->pBuffer = new_buffer;
	}
}

void EQStream::PreparePacket(EQ2Packet* app, int8 offset){
	app->setVersion(client_version);
	compressed_offset = 0;

#ifdef LE_DEBUG
	LogWrite(PACKET__DEBUG, 0, "Packet", "Before A in %s, line %i:", __FUNCTION__, __LINE__);
	DumpPacket(app);
#endif

	if(!app->packet_prepared){
		if(app->PreparePacket(MaxLen) == 255) //invalid version
			return;
	}

#ifdef LE_DEBUG
	LogWrite(PACKET__DEBUG, 0, "Packet", "After Prepare in %s, line %i:", __FUNCTION__, __LINE__);
	DumpPacket(app);
#endif

	if(!app->eq2_compressed && app->size>=0x80){
		compressed_offset = EQ2_Compress(app);
		app->eq2_compressed = true;
	}
	if(!app->packet_encrypted){
		EncryptPacket(app, compressed_offset, offset);
		if(app->size > 2 && app->pBuffer[2] == 0){
			uchar* new_buffer = new uchar[app->size+1];
			new_buffer[2] = 0;
			memcpy(new_buffer+3, app->pBuffer+2, app->size-2);
			delete[] app->pBuffer;
			app->pBuffer = new_buffer;
			app->size++;
		}
	}

#ifdef LE_DEBUG
	LogWrite(PACKET__DEBUG, 0, "Packet", "After A in %s, line %i:", __FUNCTION__, __LINE__);
	DumpPacket(app);
#endif

}

void EQStream::SendPacket(EQProtocolPacket *p)
{
	uint32 chunksize,used;
	uint32 length;

	// Convert the EQApplicationPacket to 1 or more EQProtocolPackets
	if (p->size>( MaxLen-6)) { // proto-op(2), app-op(2) ... data ... crc(2)
		uchar* tmpbuff=p->pBuffer;
		length=p->size - 2;

		EQProtocolPacket *out=new EQProtocolPacket(OP_Fragment,NULL,MaxLen-4);
		*(uint32 *)(out->pBuffer+2)=htonl(length);
		memcpy(out->pBuffer+6,tmpbuff+2,MaxLen-10);
		used=MaxLen-10;

#ifdef LE_DEBUG
		LogWrite(PACKET__DEBUG, 0, "Packet", "(%s, %i) New Fragment: ", __FUNCTION__, __LINE__);
		DumpPacket(out);
#endif

		SequencedPush(out);

		while (used<length) {
			chunksize=min(length-used,MaxLen-6);
			out=new EQProtocolPacket(OP_Fragment,NULL,chunksize+2);
			//memcpy(out->pBuffer+2,tmpbuff,1);
			memcpy(out->pBuffer+2,tmpbuff+used+2,chunksize);
#ifdef LE_DEBUG
		LogWrite(PACKET__DEBUG, 0, "Packet", "Chunk: ");
		DumpPacket(out);
#endif
			SequencedPush(out);
			used+=chunksize;

		}

#ifdef LE_DEBUG
		LogWrite(PACKET__DEBUG, 0, "Packet", "Chunk: ");
		DumpPacket(out);
		cerr << "1: Deleting 0x" << hex << (uint32)(p) << dec << endl;
#endif

		delete p;
	} else {
		SequencedPush(p);
	}
}
void EQStream::SendPacket(EQApplicationPacket *p)
{
uint32 chunksize,used;
uint32 length;

	// Convert the EQApplicationPacket to 1 or more EQProtocolPackets
	if (p->size>(MaxLen-8)) { // proto-op(2), seq(2), app-op(2) ... data ... crc(2)
		//cout << "Making oversized packet for: " << endl;
		//cout << p->size << endl;
		//p->DumpRawHeader();
		//dump_message(p->pBuffer,p->size,timestamp());
		//cout << p->size << endl;
		unsigned char *tmpbuff=new unsigned char[p->size+2];
		//cout << hex << (int)tmpbuff << dec << endl;
		length=p->serialize(tmpbuff);

		EQProtocolPacket *out=new EQProtocolPacket(OP_Fragment,NULL,MaxLen-4);
		*(uint32 *)(out->pBuffer+2)=htonl(p->Size());
		memcpy(out->pBuffer+6,tmpbuff,MaxLen-10);
		used=MaxLen-10;
		SequencedPush(out);
		//cout << "Chunk #" << ++i << " size=" << used << ", length-used=" << (length-used) << endl;
		while (used<length) {
			out=new EQProtocolPacket(OP_Fragment,NULL,MaxLen-4);
			chunksize=min(length-used,MaxLen-6);
			memcpy(out->pBuffer+2,tmpbuff+used,chunksize);
			out->size=chunksize+2;
			SequencedPush(out);
			used+=chunksize;
			//cout << "Chunk #"<< ++i << " size=" << chunksize << ", length-used=" << (length-used) << endl;
		}
		//cerr << "1: Deleting 0x" << hex << (uint32)(p) << dec << endl;
		delete p;
		delete[] tmpbuff;
	} else {
		EQProtocolPacket *out=new EQProtocolPacket(OP_Packet,NULL,p->Size()+2);
		p->serialize(out->pBuffer+2);
		SequencedPush(out);
		//cerr << "2: Deleting 0x" << hex << (uint32)(p) << dec << endl;
		delete p;
	}
}

void EQStream::SequencedPush(EQProtocolPacket *p)
{
	p->setVersion(client_version);
	MOutboundQueue.lock();
	*(uint16 *)(p->pBuffer)=htons(NextOutSeq);
	SequencedQueue.push_back(p);
	p->sequence = NextOutSeq;
	NextOutSeq++;
	MOutboundQueue.unlock();
}

void EQStream::NonSequencedPush(EQProtocolPacket *p)
{
	p->setVersion(client_version);
	MOutboundQueue.lock();
	NonSequencedQueue.push_back(p);
	MOutboundQueue.unlock();
}

void EQStream::SendAck(uint16 seq)
{
	uint16 Seq=htons(seq);
	SetLastAckSent(seq);
	NonSequencedPush(new EQProtocolPacket(OP_Ack,(unsigned char *)&Seq,sizeof(uint16)));
}

void EQStream::SendOutOfOrderAck(uint16 seq)
{
	uint16 Seq=htons(seq);
	NonSequencedPush(new EQProtocolPacket(OP_OutOfOrderAck,(unsigned char *)&Seq,sizeof(uint16)));
}

bool EQStream::CheckCombineQueue(){
	bool ret = true; //processed all packets
	MCombineQueueLock.lock();
	if(combine_queue.size() > 0){
		EQ2Packet* first = combine_queue.front();
		combine_queue.pop_front();
		if(combine_queue.size() == 0){ //nothing to combine this with
			EQ2QueuePacket(first, true);
		}
		else{
			PreparePacket(first);
			EQ2Packet* second = 0;
			bool combine_worked = false;
			int16 count = 0;
			while(combine_queue.size()){
				count++;
				second = combine_queue.front();
				combine_queue.pop_front();
				MCombineQueueLock.unlock();
				PreparePacket(second);
				/*if(first->GetRawOpcode() != OP_AppCombined && first->pBuffer[2] == 0){
					EQ2Packet* tmp = second;
					second = first;
					first = tmp;
				}*/
				if(!first->AppCombine(second)){
					first->SetProtocolOpcode(OP_Packet);
					if(combine_worked){
						SequencedPush(first);
					}
					else{
						EQ2QueuePacket(first, true);
					}
					first = second;
					combine_worked = false;
				}
				else{
					combine_worked = true;
					//DumpPacket(first);
				}
				MCombineQueueLock.lock();
				if(count >= 20){ //other clients need packets too
					ret = false;
					break;
				}
			}
			if(first){
				first->SetProtocolOpcode(OP_Packet);
				if(combine_worked){
					SequencedPush(first);
				}
				else{
					EQ2QueuePacket(first, true);
				}
			}
		}
	}
	MCombineQueueLock.unlock();
	return ret;
}

void EQStream::CheckResend(int eq_fd){
	int32 curr = Timer::GetCurrentTime2();
	EQProtocolPacket* packet = 0;
	deque<EQProtocolPacket*>::iterator itr;
	MResendQue.lock();
	for(itr=resend_que.begin();itr!=resend_que.end();itr++){
		packet = *itr;
		if(packet->attempt_count >= 5){//tried to resend this packet 5 times, client must already have it but didnt ack it
			safe_delete(packet);
			itr = resend_que.erase(itr);
			if(itr == resend_que.end())
				break;
		}
		else{
			if((curr - packet->sent_time) < 1000)
				continue;
			packet->sent_time -=1000;
			packet->attempt_count++;
			WritePacket(eq_fd, packet);
		}
	}
	MResendQue.unlock();
}

void EQStream::Write(int eq_fd)
{
	deque<EQProtocolPacket *> ReadyToSend;
	deque<EQProtocolPacket *> SeqReadyToSend;
	long maxack;
	map<uint16, EQProtocolPacket *>::iterator sitr;

	// Check our rate to make sure we can send more
	MRate.lock();
	sint32 threshold=RateThreshold;
	MRate.unlock();
	if (BytesWritten > threshold) {
		//cout << "Over threshold: " << BytesWritten << " > " << threshold << endl;
		return;
	}

	MCombinedAppPacket.lock();
	EQApplicationPacket *CombPack=CombinedAppPacket;
	CombinedAppPacket=NULL;
	MCombinedAppPacket.unlock();

	if (CombPack) {
		SendPacket(CombPack);
	}

	// If we got more packets to we need to ack, send an ack on the highest one
	MAcks.lock();
	maxack=MaxAckReceived;
	if (NextAckToSend>LastAckSent)
		SendAck(NextAckToSend);
	MAcks.unlock();

	// Lock the outbound queues while we process
	MOutboundQueue.lock();

	// Adjust where we start sending in case we get a late ack
	if (maxack>LastSeqSent)
		LastSeqSent=maxack;

	// Place to hold the base packet t combine into
	EQProtocolPacket *p=NULL;

	// Loop until both are empty or MaxSends is reached
	
	// See if there are more non-sequenced packets left
	while (NonSequencedQueue.size()) {
		if (!p) {
			// If we don't have a packet to try to combine into, use this one as the base
			// And remove it form the queue
			p = NonSequencedQueue.front();
			NonSequencedQueue.pop_front();
		}
		//Check if we have a packet to combine p with...
		if (NonSequencedQueue.size()){
			if (!p->combine(NonSequencedQueue.front())) {
				// Tryint to combine this packet with the base didn't work (too big maybe)
				// So just send the base packet (we'll try this packet again later)
				ReadyToSend.push_back(p);
				BytesWritten += p->size;
				p = NULL;
			}
			else {
				// Combine worked, so just remove this packet and it's spot in the queue
				delete NonSequencedQueue.front();
				NonSequencedQueue.pop_front();
			}
		}
		else {
			//We have no packets to combine p with so just send it...
			ReadyToSend.push_back(p);
			BytesWritten += p->size;
			p = NULL;
		}
		if (BytesWritten > threshold) {
			// Sent enough this round, lets stop to be fair
			break;
		}
	}

	//The non-seq loop must have broke before we sent this packet, send it now
	if (p){
		ReadyToSend.push_back(p);
		BytesWritten += p->size;
	}

	if (SequencedQueue.size() && BytesWritten < threshold) {
		while(SequencedQueue.size()) {
			p = SequencedQueue.front();
			BytesWritten+=p->size;
			SeqReadyToSend.push_back(p);
			p->sent_time = Timer::GetCurrentTime2();
			resend_que.push_back(p);
			SequencedQueue.pop_front();
			LastSeqSent=p->sequence;
			if (BytesWritten > threshold) {
				break;
			}
		}
	}
			
	// Unlock the queue
	MOutboundQueue.unlock();

	// Send all the packets we "made"
	while(ReadyToSend.size()) {
		WritePacket(eq_fd,ReadyToSend.front());
		delete ReadyToSend.front();
		ReadyToSend.pop_front();
	}

	while(SeqReadyToSend.size()){
		WritePacket(eq_fd,SeqReadyToSend.front());
		SeqReadyToSend.pop_front();
	}
}

void EQStream::WritePacket(int eq_fd, EQProtocolPacket *p)
{
uint32 length = 0;
sockaddr_in address;
unsigned char tmpbuffer[1024];
	address.sin_family = AF_INET;
	address.sin_addr.s_addr=remote_ip;
	address.sin_port=remote_port;
#ifdef NOWAY
	uint32 ip=address.sin_addr.s_addr;
	cout << "Sending to: " 
		<< (int)*(unsigned char *)&ip
		<< "." << (int)*((unsigned char *)&ip+1)
		<< "." << (int)*((unsigned char *)&ip+2)
		<< "." << (int)*((unsigned char *)&ip+3)
		<< "," << (int)ntohs(address.sin_port) << "(" << p->size << ")" << endl;

	p->DumpRaw();
	cout << "-------------" << endl;
#endif
	length=p->serialize(buffer);
	if (p->opcode!=OP_SessionRequest && p->opcode!=OP_SessionResponse) {
		if (compressed) {
			uint32 newlen=EQProtocolPacket::Compress(buffer,length,tmpbuffer,1024);
			memcpy(buffer,tmpbuffer,newlen);
			length=newlen;
		}
		if (encoded) {
			EQProtocolPacket::ChatEncode(buffer,length,Key);
		}
		*(uint16 *)(buffer+length)=htons(CRC16(buffer,length,Key));
		length+=2;
	}
	sent_packets++;
	//dump_message_column(buffer,length,"Writer: ");
	//cout << "Raw Data:\n";
	//DumpPacket(buffer, length);
	sendto(eq_fd,(char *)buffer,length,0,(sockaddr *)&address,sizeof(address));
}

EQProtocolPacket *EQStream::Read(int eq_fd, sockaddr_in *from)
{
int socklen;
int length=0;
unsigned char buffer[2048];
EQProtocolPacket *p=NULL;
char temp[15];

	socklen=sizeof(sockaddr);
#ifdef WIN32
	length=recvfrom(eq_fd, (char *)buffer, 2048, 0, (struct sockaddr*)from, (int *)&socklen);
#else
	length=recvfrom(eq_fd, buffer, 2048, 0, (struct sockaddr*)from, (socklen_t *)&socklen);
#endif
	if (length>=2) {
		DumpPacket(buffer, length);
		p=new EQProtocolPacket(buffer[1],&buffer[2],length-2);

		uint32 ip=from->sin_addr.s_addr;
		sprintf(temp,"%d.%d.%d.%d:%d",
			*(unsigned char *)&ip,
			*((unsigned char *)&ip+1),
			*((unsigned char *)&ip+2),
			*((unsigned char *)&ip+3),
			ntohs(from->sin_port));
		//cout << timestamp() << "Data from: " << temp << " OpCode 0x" << hex << setw(2) << setfill('0') << (int)p->opcode << dec << endl;
		//dump_message(p->pBuffer,p->size,timestamp());
		
	}
	return p;
}

void EQStream::SendSessionResponse()
{
EQProtocolPacket *out=new EQProtocolPacket(OP_SessionResponse,NULL,sizeof(SessionResponse));
	SessionResponse *Response=(SessionResponse *)out->pBuffer;
	Response->Session=htonl(Session);
	Response->MaxLength=htonl(MaxLen);
	Response->UnknownA=2;
	Response->Format=0;
	if (compressed)
		Response->Format|=FLAG_COMPRESSED;
	if (encoded)
		Response->Format|=FLAG_ENCODED;
	Response->Key=htonl(Key);

	out->size=sizeof(SessionResponse);

	NonSequencedPush(out);
}

void EQStream::SendSessionRequest()
{
	EQProtocolPacket *out=new EQProtocolPacket(OP_SessionRequest,NULL,sizeof(SessionRequest));
	SessionRequest *Request=(SessionRequest *)out->pBuffer;
	memset(Request,0,sizeof(SessionRequest));
	Request->Session=htonl(time(NULL));
	Request->MaxLength=htonl(512);

	NonSequencedPush(out);
}

void EQStream::SendDisconnect(bool setstate)
{
	try{
		if(GetState() != ESTABLISHED)
			return;
		
		EQProtocolPacket *out=new EQProtocolPacket(OP_SessionDisconnect,NULL,sizeof(uint32)+sizeof(int16));
		*(uint32 *)out->pBuffer=htonl(Session);
		out->pBuffer[4] = 0;
		out->pBuffer[5] = 6;
		NonSequencedPush(out);
		if(setstate)
			SetState(CLOSING);
	}
	catch(...){}
}

void EQStream::InboundQueuePush(EQApplicationPacket *p)
{
	MInboundQueue.lock();
	InboundQueue.push_back(p);
	MInboundQueue.unlock();
}

EQApplicationPacket *EQStream::PopPacket()
{
EQApplicationPacket *p=NULL;

	MInboundQueue.lock();
	if (InboundQueue.size()) {
		p=InboundQueue.front();
		InboundQueue.pop_front();
	}
	MInboundQueue.unlock();
	if(p)
		p->setVersion(client_version);
	return p;
}

void EQStream::InboundQueueClear()
{
	MInboundQueue.lock();
	while(InboundQueue.size()){
		delete InboundQueue.front();
		InboundQueue.pop_front();
	}
	MInboundQueue.unlock();
}
void EQStream::EncryptPacket(uchar* data, int16 size){
	if(size>6){
		
	}
}
bool EQStream::HasOutgoingData()
{
bool flag;
	
	//once closed, we have nothing more to say
	if(CheckClosed())
		return(false);
	
	MOutboundQueue.lock();
	flag=(!NonSequencedQueue.empty());
	if (!flag) {
		flag = (!SequencedQueue.empty());
	}
	MOutboundQueue.unlock();

	if (!flag) {
		MAcks.lock();
		flag= (NextAckToSend>LastAckSent);
		MAcks.unlock();
	}

	if (!flag) {
		MCombinedAppPacket.lock();
		flag=(CombinedAppPacket!=NULL);
		MCombinedAppPacket.unlock();
	}

	return flag;
}

void EQStream::OutboundQueueClear()
{
	MOutboundQueue.lock();
	while(NonSequencedQueue.size()) {
		delete NonSequencedQueue.front();
		NonSequencedQueue.pop_front();
	}
	while(SequencedQueue.size()) {
		delete SequencedQueue.front();
		SequencedQueue.pop_front();
	}
	MOutboundQueue.unlock();
}

void EQStream::Process(const unsigned char *buffer, const uint32 length)
{
	received_packets++;
static unsigned char newbuffer[2048];
uint32 newlength=0;
	if (EQProtocolPacket::ValidateCRC(buffer,length,Key)) {
		if (compressed) {
			newlength=EQProtocolPacket::Decompress(buffer,length,newbuffer,2048);
		} else {
			memcpy(newbuffer,buffer,length);
			newlength=length;
			if (encoded)
				EQProtocolPacket::ChatDecode(newbuffer,newlength-2,Key);
		}
		if (buffer[1]!=0x01 && buffer[1]!=0x02 && buffer[1]!=0x1d)
			newlength-=2;
		EQProtocolPacket p(newbuffer,newlength);
		ProcessPacket(&p);
	} else {
#ifdef EQN_DEBUG
		cout << "Incoming packet failed checksum:" <<endl;
		dump_message_column(const_cast<unsigned char *>(buffer),length,"CRC failed: ");
#endif
	}
}

long EQStream::GetMaxAckReceived()
{
	MAcks.lock();
	long l=MaxAckReceived;
	MAcks.unlock();

	return l;
}

long EQStream::GetNextAckToSend()
{
	MAcks.lock();
	long l=NextAckToSend;
	MAcks.unlock();

	return l;
}

long EQStream::GetLastAckSent()
{
	MAcks.lock();
	long l=LastAckSent;
	MAcks.unlock();

	return l;
}

void EQStream::SetMaxAckReceived(uint32 seq)
{
	deque<EQProtocolPacket *>::iterator itr;

	MAcks.lock();
	MaxAckReceived=seq;
	MAcks.unlock();
	MOutboundQueue.lock();
	if (long(seq) > LastSeqSent)
		LastSeqSent=seq;
	MResendQue.lock();
	EQProtocolPacket* packet = 0;
	for(itr=resend_que.begin();itr!=resend_que.end();itr++){
		packet = *itr;
		if(packet && packet->sequence <= seq){
			safe_delete(packet);
			itr = resend_que.erase(itr);
			if(itr == resend_que.end())
				break;
		}
	}
	MResendQue.unlock();
	MOutboundQueue.unlock();
}

void EQStream::SetNextAckToSend(uint32 seq)
{
	MAcks.lock();
	NextAckToSend=seq;
	MAcks.unlock();
}

void EQStream::SetLastAckSent(uint32 seq)
{
	MAcks.lock();
	LastAckSent=seq;
	MAcks.unlock();
}

void EQStream::SetLastSeqSent(uint32 seq)
{
	MOutboundQueue.lock();
	LastSeqSent=seq;
	MOutboundQueue.unlock();
}

void EQStream::SetStreamType(EQStreamType type)
{
	StreamType=type;
	switch (StreamType) {
		case LoginStream:
			app_opcode_size=1;
			compressed=false;
			encoded=false;
			break;
		case EQ2Stream:
			app_opcode_size=2;
			compressed=false;
			encoded=false;
			break;
		case ChatOrMailStream:
		case ChatStream:
		case MailStream:
			app_opcode_size=1;
			compressed=false;
			encoded=true;
			break;
		case ZoneStream:
		case WorldStream:
		default:
			app_opcode_size=2;
			compressed=true;
			encoded=false;
			break;
	}
}

sint8 EQStream::CompareSequence(uint16 expected_seq , uint16 seq)
{
	if (expected_seq==seq) {
		// Curent
		return 0;
	}  else if ((seq > expected_seq && (uint32)seq < ((uint32)expected_seq + EQStream::MaxWindowSize)) || seq < (expected_seq - EQStream::MaxWindowSize)) {
		// Future
		return 1;
	} else {
		// Past
		return -1;
	}
}

void EQStream::Decay()
{
	MRate.lock();
	uint32 rate=DecayRate;
	MRate.unlock();
	if (BytesWritten>0) {
		BytesWritten-=rate;
		if (BytesWritten<0)
			BytesWritten=0;
	}
}

void EQStream::AdjustRates(uint32 average_delta)
{
	if (average_delta) {
		MRate.lock();
		RateThreshold=RATEBASE/average_delta;
		DecayRate=DECAYBASE/average_delta;
		MRate.unlock();
	}
}
