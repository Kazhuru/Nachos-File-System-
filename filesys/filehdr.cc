// filehdr.cc
//	Routines for managing the disk file header (in UNIX, this
//	would be called the i-node).
//
//	The file header is used to locate where on disk the
//	file's data is stored.  We implement this as a fixed size
//	table of pointers -- each entry in the table points to the
//	disk sector containing that portion of the file data
//	(in other words, there are no indirect or doubly indirect
//	blocks). The table size is chosen so that the file header
//	will be just big enough to fit in one disk sector,
//
//      Unlike in a real system, we do not keep track of file permissions,
//	ownership, last modification date, etc., in the file header.
//
//	A file header can be initialized in two ways:
//	   for a new file, by modifying the in-memory data structure
//	     to point to the newly allocated data blocks
//	   for a file already on disk, by reading the file header from disk
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "system.h"
#include "filehdr.h"

//----------------------------------------------------------------------
// FileHeader::Allocate
// 	Initialize a fresh file header for a newly created file.
//	Allocate data blocks for the file out of the map of free disk blocks.
//	Return FALSE if there are not enough free blocks to accomodate
//	the new file.
//
//	"freeMap" is the bit map of free disk sectors
//	"fileSize" is the file length
//----------------------------------------------------------------------

void
FileHeader::Init()
{
  for (int i = 0; i < NumDirect+2; i++) {
    dataIndSectors[i] = -1;
  }
  for (int i = 0; i < NumDirect+2; i++) {
    for (int j = 0; j < NumDirect+2; j++) {
      dataDobSectors[i][j] = -1;
    }
  }
  for (int i = 0; i < NumDirect+2; i++) {
    dataDobIndex[i] = -1;
  }
}

bool
FileHeader::Allocate(BitMap *freeMap, int fileSize)
{
    numBytes = fileSize;
    numSectors  = divRoundUp(fileSize, SectorSize);
    if (freeMap->NumClear() < numSectors)
	     return FALSE;		// not enough space

   Init();

   if(numSectors <= NumDirect)
   {   //Directos
       printf("Creando Apt Directos\n" );
       for (int i = 0; i < numSectors; i++)
         dataSectors[i] = freeMap->Find();
   }
   else if(numSectors > NumDirect && numSectors <= NUM_IND)
     {    //Indirecto Sencillos restantes de los Directos.
          printf("Creando Apt Indirectos\n" );
          int numIndirect = numSectors - NumDirect-1;
          for (int i = 0; i < NumDirect; i++) {
            dataSectors[i] = freeMap->Find();
          }
          for (int i = 0; i < numIndirect; i++) {
            dataIndSectors[i] = freeMap->Find();
          }
     }
     else {
      printf("Creando Apt Dobles\n" );
      int numDouble = numSectors - (NUM_IND+1);

      for (int i = 0; i < NumDirect; i++) {
        dataSectors[i] = freeMap->Find();
      }
      for (int i = 0; i < NumDirect+2; i++) {
        dataIndSectors[i] = freeMap->Find();
      }

      int nLoops = divRoundUp(numDouble,(NumDirect+2));
      for (int i = 0; i < nLoops; i++) {
        dataDobIndex[i] = freeMap->Find();
      }

      for (int i = 0; i < numDouble; i++) {
        int colum = i%(NumDirect+2);
        int row = i/(NumDirect+2);
        dataDobSectors[row][colum] = freeMap->Find();
      }
     }

    return TRUE;
}

//----------------------------------------------------------------------
// FileHeader::Deallocate
// 	De-allocate all the space allocated for data blocks for this file.
//
//	"freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------

void
FileHeader::Deallocate(BitMap *freeMap)
{
  Init();
  
  if(numSectors <= NumDirect)
  {   //Directos
      printf("Eliminacion de Directos\n");
      for (int i = 0; i < numSectors; i++) {
        ASSERT(freeMap->Test((int) dataSectors[i]));  // ought to be marked!
        freeMap->Clear((int) dataSectors[i]);
      }
  }
  else if(numSectors > NumDirect && numSectors <= NUM_IND)
    {    //Indirecto Sencillos restantes de los Directos.
      printf("Eliminacion de Indirectos\n");
      for (int i = 0; i < NumDirect+2; i++) {
        if(dataIndSectors[i] != -1)
        {
          ASSERT(freeMap->Test((int) dataIndSectors[i]));  // ought to be marked!
          freeMap->Clear((int) dataIndSectors[i]);
        }

      }
      for (int i = 0; i < NumDirect; i++) {
        ASSERT(freeMap->Test((int) dataSectors[i]));  // ought to be marked!
        freeMap->Clear((int) dataSectors[i]);
      }
    }
    else {
        printf("Eliminacion de Dobles\n");
        for (int i = 0; i < NumDirect+2; i++) {
          for (int j = 0; j < NumDirect+2; j++) {
            if(dataDobSectors[i][j] != -1)
            {
              printf("dtDobSector: %d \n", dataDobSectors[i][j]);
              ASSERT(freeMap->Test((int) dataDobSectors[i][j]));
              freeMap->Clear((int) dataDobSectors[i][j]);
            }
          }
        }

        for (int i = 0; i < NumDirect+2; i++) {
          if(dataIndSectors[i] != -1)
          {
            ASSERT(freeMap->Test((int) dataIndSectors[i]));
            freeMap->Clear((int) dataIndSectors[i]);
          }
          if(dataDobIndex[i] != -1)
          {
            ASSERT(freeMap->Test((int) dataDobIndex[i]));
            freeMap->Clear((int) dataDobIndex[i]);
          }
        }

        for (int i = 0; i < NumDirect; i++) {
          ASSERT(freeMap->Test((int) dataSectors[i]));
          freeMap->Clear((int) dataSectors[i]);
        }
    }
}

//----------------------------------------------------------------------
// FileHeader::FetchFrom
// 	Fetch contents of file header from disk.
//
//	"sector" is the disk sector containing the file header
//----------------------------------------------------------------------

void
FileHeader::FetchFrom(int sector)
{
    synchDisk->ReadSector(sector, (char *)this);
    if(numSectors > NumDirect && numSectors <= NUM_IND)
    {    //Indirecto Sencillos restantes de los Directos.
        synchDisk->ReadSector(dataSectors[NumDirect-1], (char *)dataIndSectors);
    }
    else
    {   //dobles
        synchDisk->ReadSector(dataSectors[NumDirect-1], (char *)dataIndSectors);
        synchDisk->ReadSector(dataSectors[NumDirect-2], (char *)dataDobIndex);
        for (int i = 0; i < NumDirect +2; i++) {
          if(dataDobIndex[i] != -1)
          {
              synchDisk->ReadSector(dataDobIndex[i], (char *)dataDobSectors[i]);
              /*for (int j = 0; j < NumDirect +2; j++) {
                printf("dtDobSector: %d \n", dataDobSectors[i][j]);
              }*/
          }
        }

    }
}

//----------------------------------------------------------------------
// FileHeader::WriteBack
// 	Write the modified contents of the file header back to disk.
//
//	"sector" is the disk sector to contain the file header
//----------------------------------------------------------------------

void
FileHeader::WriteBack(int sector)
{   //directos
    synchDisk->WriteSector(sector, (char *)this);

    if(dataIndSectors[0] != -1)
    { //indirectos
      synchDisk->WriteSector(dataSectors[NumDirect-1], (char *)dataIndSectors);
      if(dataDobSectors[0][0] != -1)
      { //doublePointers

        /*int *doublePointers[NumDirect + 2];
        for (int i = 0; i < NumDirect+2; i++) {
          doublePointers[i] = dataDobSectors[i];
        }*/

        synchDisk->WriteSector(dataSectors[NumDirect-2], (char *)dataDobIndex);

        for (int i = 0; i < NumDirect+2; i++) {
          if(dataDobSectors[i][0] != -1)
          {
            synchDisk->WriteSector(dataDobIndex[i], (char *)dataDobSectors[i]);
          }
        }
      }
    }
}

//----------------------------------------------------------------------
// FileHeader::ByteToSector
// 	Return which disk sector is storing a particular byte within the file.
//      This is essentially a translation from a virtual address (the
//	offset in the file) to a physical address (the sector where the
//	data at the offset is stored).
//
//	"offset" is the location within the file of the byte in question
//----------------------------------------------------------------------

int
FileHeader::ByteToSector(int offset)
{
    return(dataSectors[offset / SectorSize]);
}

//----------------------------------------------------------------------
// FileHeader::FileLength
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------

int
FileHeader::FileLength()
{
    return numBytes;
}

//----------------------------------------------------------------------
// FileHeader::Print
// 	Print the contents of the file header, and the contents of all
//	the data blocks pointed to by the file header.
//----------------------------------------------------------------------

void
FileHeader::Print()
{
    int i, j, k;
    char *data = new char[SectorSize];

    printf("FileHeader contents.  File size: %d.  File blocks:\n", numBytes);
    for (i = 0; i < numSectors; i++)
	printf("%d ", dataSectors[i]);
    printf("\nFile contents:\n");
    for (i = k = 0; i < numSectors; i++) {
	synchDisk->ReadSector(dataSectors[i], data);
        for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
	    if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
		printf("%c", data[j]);
            else
		printf("\\%x", (unsigned char)data[j]);
	}
        printf("\n");
    }
    delete [] data;
}

void
FileHeader::PrintUsedSectors()
{
  for (int i = 0; i < numSectors; i++)
    printf("%d ", dataSectors[i]);
}
