#ifndef IRQLIST_H
#define IRQLIST_H

#pragma pack(1)
typedef struct _IRQDESCR {
  USHORT     usNextOffs;
  USHORT     usIRQLevel;
  CHAR       acDescr[1];
} IRQDESCR, *PIRQDESCR;
#pragma pack()

PIRQDESCR irqGetList();
VOID irqFreeList(PIRQDESCR pIRQDescr);
PSZ irqGetDescription(PIRQDESCR pIRQDescr, USHORT usIRQLevel);

#endif // IRQLIST_H
