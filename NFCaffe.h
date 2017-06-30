/*
 * NFCaffe.h
 *
 *  Created on: May 7, 2017
 *      Author: COX
 */

#ifndef NFCAFFE_H_
#define NFCAFFE_H_

#include "PN532.h"
#include "Utils.h"

void SM_CardReading(void);
void LongDelay(int s32_Interval);
bool ReadCard(byte u8_UID[8], kCard* pk_Card);
void GetSDCounterForCard(char* fileName, uint16_t * u16_noOfCoffees);
void UpdateSDCardCounter(uint64_t u64_ID, kCard* pk_Card, uint64_t u64_StartTick);

#endif /* NFCAFFE_H_ */
