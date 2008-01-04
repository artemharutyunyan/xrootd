/******************************************************************************/
/*                                                                            */
/*                          X r d C m s K e y . c c                           */
/*                                                                            */
/* (c) 2007 by the Board of Trustees of the Leland Stanford, Jr., University  */
/*                            All Rights Reserved                             */
/*   Produced by Andrew Hanushevsky for Stanford University under contract    */
/*              DE-AC02-76-SFO0515 with the Department of Energy              */
/******************************************************************************/

//         $Id$

#include <errno.h>
#include <string.h>

#include "XrdCms/XrdCmsKey.hh"
#include "XrdCms/XrdCmsTrace.hh"
#include "XrdOuc/XrdOucCRC.hh"
#include "XrdSys/XrdSysError.hh"

using namespace XrdCms;

/******************************************************************************/
/*                       C l a s s   X r d C m s K e y                        */
/******************************************************************************/
/******************************************************************************/
/* public                        s e t H a s h                                */
/******************************************************************************/
  
void XrdCmsKey::setHash()
{
     if (!Len) Len = strlen(Val);
     if (!(Hash = XrdOucCRC::CRC32((const unsigned char *)Val, Len))) Hash = 1;
}

/******************************************************************************/
/*                   C l a s s   X r d C m s K e y I t e m                    */
/******************************************************************************/
/******************************************************************************/
/*                           S t a t i c   D a t a                            */
/******************************************************************************/
  
XrdCmsKeyItem *XrdCmsKeyItem::TockTable[TickRate] = {0};
XrdCmsKeyItem *XrdCmsKeyItem::Free    = 0;
int            XrdCmsKeyItem::numFree = 0;
int            XrdCmsKeyItem::numHave = 0;
int            XrdCmsKeyItem::numNull = 0;

/******************************************************************************/
/* static public                   A l l o c                                  */
/******************************************************************************/
  
XrdCmsKeyItem *XrdCmsKeyItem::Alloc(unsigned int theTock)
{
  XrdCmsKeyItem *kP;

// Try to allocate an existing item or replenish the list
//
   do {if ((kP = Free))
          {Free = kP->Next;
           numFree--;
           theTock &= TickMask;
           kP->Key.TODRef = TockTable[theTock];
           TockTable[theTock] = kP;
           if (!(kP->Key.Ref++)) kP->Key.Ref = 1;
           return kP;
          }
       numNull++;
       } while(Replenish());

// We failed
//
   Say.Emsg("Key", ENOMEM, "create key item");
   return (XrdCmsKeyItem *)0;
}

/******************************************************************************/
/* public                        R e c y c l e                                */
/******************************************************************************/
  
void XrdCmsKeyItem::Recycle()
{

// Clear up data areas
//
   if (Key.Val) {free(Key.Val); Key.Val = (char *)"";}
   Key.Hash = 0; Key.Ref = 0;

// Put entry on the free list
//
   Next = Free; Free = this;
   numFree++;
}

/******************************************************************************/
/* public                         R e l o a d                                 */
/******************************************************************************/
  
void XrdCmsKeyItem::Reload()
{
   Key.TOD &= static_cast<unsigned char>(TickMask);
   Key.TODRef = TockTable[Key.TOD];
   TockTable[Key.TOD] = this;
}

/******************************************************************************/
/* static public               R e p l e n i s h                              */
/******************************************************************************/

int XrdCmsKeyItem::Replenish()
{
   EPNAME("Replenish");
   XrdCmsKeyItem *kP;
   int i;

// Allocate a quantum of free elements and chain them into the free list
//
   if (!(kP = new XrdCmsKeyItem[minAlloc])) return 0;
   DEBUG("old free " <<numFree <<" + " <<minAlloc <<" = " <<numHave+minAlloc);
  
// Put them on the free list but return the last one allocated
//
   numHave += minAlloc;
   for (i = 0; i < minAlloc; i++)
       {kP->Next = Free; Free = kP; kP++;}
   numFree += minAlloc;
   return numFree;
}

/******************************************************************************/
/* static public                   S t a t s                                  */
/******************************************************************************/

void XrdCmsKeyItem::Stats(int &isAlloc, int &isFree, int &wasNull)
{

   isAlloc  = numHave;
   isFree   = numFree;
   wasNull  = numNull;
   numNull  = 0;
}

/******************************************************************************/
/* static public                  U n l o a d                                 */
/******************************************************************************/
  
XrdCmsKeyItem *XrdCmsKeyItem::Unload(unsigned int theTock)
{
   XrdCmsKeyItem  myItem, *nP, *pP = &myItem;

// Remove all entries from the indicated list. If any entries have been
// reassigned to a different list, move them to the right list. Otherwise,
// make the entry unfindable by clearing the hash code. Since item recycling
// requires knowing the hash code, we save it elsewhere in the object.
//
   theTock &= TickMask;
   myItem.Key.TODRef = TockTable[theTock]; TockTable[theTock] = 0;
   while((nP = pP->Key.TODRef))
         if (nP->Key.TOD == theTock) 
            {nP->Loc.HashSave = nP->Key.Hash; nP->Key.Hash = 0; pP = nP;}
            else {pP->Key.TODRef = nP->Key.TODRef;
                  nP->Key.TODRef = TockTable[nP->Key.TOD];
                  TockTable[nP->Key.TOD] = nP;
                 }
   return myItem.Key.TODRef;
}

/******************************************************************************/
  
XrdCmsKeyItem *XrdCmsKeyItem::Unload(XrdCmsKeyItem *theItem)
{
   XrdCmsKeyItem *kP, *pP = 0;
   unsigned int theTock = theItem->Key.TOD & TickMask;

// Remove the entry from the right list
//
   kP = TockTable[theTock];
   while(kP && kP != theItem) {pP = kP; kP = kP->Key.TODRef;}
   if (kP)
      {if (pP) pP->Key.TODRef     = kP->Key.TODRef;
          else TockTable[theTock] = kP->Key.TODRef;
      }
   return kP;
}
