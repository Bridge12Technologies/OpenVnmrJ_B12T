/*
 * Copyright (C) 2015  University of Oregon
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Apache License, as specified in the LICENSE file.
 *
 * For more information, see the LICENSE file.
 */

#ifndef MPS_H
#define MPS_H

extern int  openMPS(void);
extern int  sendMPS(const char *msg);
extern int  recvMPS(char *msg, size_t len);
extern void closeMPS(void);
extern void statusMPS(void);
extern int  getStatrateMPS();
extern int  getTuneFlag();
extern void statusCheckMPS(int sig);
extern void statrateMPS(int msec);
extern void acqMPS(int stage);
extern void mpsMode(char *mode);
extern void mpsPower(int power);
extern int  mpsDataPt(int freq, int ms);
extern void mpsTuneData(int init, char *outfile0, int np0);
extern void defaultStatrateMPS(void);
extern void errorMpsRfstat(char *msg);

#endif
