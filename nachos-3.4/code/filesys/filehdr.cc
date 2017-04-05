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
//	"fileSize" is the bit map of free disk sectors
//----------------------------------------------------------------------

bool
FileHeader::Allocate(BitMap *freeMap, int fileSize) //TODO i think were getting old stuff in our last sector... how to fix?
{ 
    numBytes = fileSize;
    numSectors = divRoundUp(fileSize, SectorSize);
    if (freeMap->NumClear() < numSectors)
	    return FALSE;		// not enough space

    for (int i = 0; i < NumDirect; i++){
        dataSectors[i] = 0;
    }
    
    if (numBytes <= MaxFileSize){           //use Type 1
        for (int i = 0; i < numSectors; i++)
            dataSectors[i] = freeMap->Find();

    } else if (numBytes <= MaxFileSize2){   //use Type 2      
        for (int i = 0; i < NumDirect; i++)
            dataSectors[i] = freeMap->Find();

        int dataSectors2 [NumDirect + 2];
        for (int i = 0; i < numSectors - (NumDirect - 2); i++)
            dataSectors2[i] = freeMap->Find();
        synchDisk->WriteSector(dataSectors[NumDirect - 1], (char *)dataSectors2);

    } else if (numBytes <= MaxFileSize3){   //use Type 3
        int counter = 0;

        for (int i = 0; i < NumDirect; i++){
            dataSectors[i] = freeMap->Find();;
            if (i < NumDirect -2) counter++;
        }

        int dataSectors2 [NumDirect + 2];
        for (int i = 0; i < NumDirect + 2; i++, counter++){
            dataSectors2[i] = freeMap->Find();
        }
        synchDisk->WriteSector(dataSectors[NumDirect - 2], (char *)dataSectors2);

       int dataSectors3 [NumDirect + 2];
        for (int i = 0; counter < numSectors; i++){
            dataSectors3[i] = freeMap->Find();
            
            int sectors [NumDirect + 2];
            for (int j = 0; counter < numSectors && j < NumDirect + 2; j++, counter++){
                sectors[j] = freeMap->Find();
            }
            synchDisk->WriteSector(dataSectors3[i], (char *)sectors);
        }
        synchDisk->WriteSector(dataSectors[NumDirect - 1], (char *)dataSectors3);

    } else return FALSE; //file too big
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
    if (numBytes <= MaxFileSize){
        for (int i = 0; i < numSectors; i++) {
            ASSERT(freeMap->Test((int) dataSectors[i]));  // ought to be marked!
            freeMap->Clear((int) dataSectors[i]);
        }

    } else if (numBytes <= MaxFileSize2){
        int dataSectors2 [NumDirect + 2];
        synchDisk->ReadSector(dataSectors[NumDirect - 1], (char*)dataSectors2);

        for (int i = 0; i < numSectors - (NumDirect - 1); i++){
            ASSERT(freeMap->Test((int) dataSectors2[i]));  // ought to be marked!
            freeMap->Clear((int) dataSectors2[i]);
        }

        for (int i = 0; i < NumDirect; i++) {
            ASSERT(freeMap->Test((int) dataSectors[i]));  // ought to be marked!
            freeMap->Clear((int) dataSectors[i]);
        }

    } else {
        int dataSectors2 [NumDirect + 2];
        synchDisk->ReadSector(dataSectors[NumDirect - 2], (char*)dataSectors2);
        int dataSectors3 [NumDirect + 2];
        synchDisk->ReadSector(dataSectors[NumDirect - 1], (char*)dataSectors3);

        int counter = 2* NumDirect;
        for (int i = 0; counter < numSectors; i++){
            int sectors [NumDirect + 2];
            synchDisk->ReadSector(dataSectors3[i], (char*)sectors);

            for (int j = 0; j < NumDirect + 2 && counter < numSectors; j++){
                ASSERT(freeMap->Test((int) sectors[j]));  // ought to be marked!
                freeMap->Clear((int) sectors[j]);
                counter++;
            }

            ASSERT(freeMap->Test((int) dataSectors3[i]));  // ought to be marked!
            freeMap->Clear((int) dataSectors3[i]);
        }

        for (int i = 0; i < NumDirect + 2; i++){
            ASSERT(freeMap->Test((int) dataSectors2[i]));  // ought to be marked!
            freeMap->Clear((int) dataSectors2[i]);
        }

        for (int i = 0; i < NumDirect; i++) {
            ASSERT(freeMap->Test((int) dataSectors[i]));  // ought to be marked!
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
}

//----------------------------------------------------------------------
// FileHeader::WriteBack
// 	Write the modified contents of the file header back to disk. 
//
//	"sector" is the disk sector to contain the file header
//----------------------------------------------------------------------

void
FileHeader::WriteBack(int sector)
{
    synchDisk->WriteSector(sector, (char *)this); 
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
    int sector = offset / SectorSize;

    if (numBytes <= MaxFileSize)        //type 1
        return(dataSectors[sector]);

    else if (numBytes <= MaxFileSize2){ //type 2
        if(sector < NumDirect - 1)          //sector < 61 cause 61 is indirect
            return(dataSectors[sector]);
        
        int dataSectors2 [NumDirect + 2];
        synchDisk->ReadSector(dataSectors[NumDirect - 1], (char*)dataSectors2);
        return(dataSectors2[sector - (NumDirect - 1)]);   //sector - (62 - 1) = sector - 61

    } else{                             //type 3
        if(sector < NumDirect - 2)
            return(dataSectors[sector]);

        if(sector < 2*NumDirect){
            int dataSectors2 [NumDirect + 2];
            synchDisk->ReadSector(dataSectors[NumDirect - 2], (char*)dataSectors2);
            return(dataSectors2[sector - (NumDirect - 2)]);
        }

        int dataSectors3 [NumDirect + 2];
        synchDisk->ReadSector(dataSectors[NumDirect - 1], (char*)dataSectors3);

        int sectors [NumDirect + 2];
        synchDisk->ReadSector(dataSectors3[(sector - 2*NumDirect)/(NumDirect + 2)], (char*)sectors);

        return(sectors[(sector - 2*NumDirect)%(NumDirect + 2)]);
    }
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

    if (numBytes <= MaxFileSize){       //type 1
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

    } else if (numBytes <= MaxFileSize2){ //type 2
        int dataSectors2 [NumDirect + 2];
        synchDisk->ReadSector(dataSectors[NumDirect - 1], (char*)dataSectors2);
        
        printf("FileHeader contents.  File size: %d.  File blocks:\n", numBytes);
        for (i = 0; i < NumDirect; i++)
            printf("%d ", dataSectors[i]);
        printf("\nFile contents:\n");

        for (i = k = 0; i < NumDirect - 1; i++) {
            synchDisk->ReadSector(dataSectors[i], data);
            for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
                if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
                    printf("%c", data[j]);
                else
                    printf("\\%x", (unsigned char)data[j]);
            }
            printf("\n"); 
        }

        for (i = 0; i < numSectors - (NumDirect - 1); i++) {
            synchDisk->ReadSector(dataSectors2[i], data);
            for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
                if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
                    printf("%c", data[j]);
                else
                    printf("\\%x", (unsigned char)data[j]);
            }
            printf("\n"); 
        }

    } else{                             //type 3        not working completely
        int counter = 0;
        
        int dataSectors2 [NumDirect + 2];
        synchDisk->ReadSector(dataSectors[NumDirect - 2], (char*)dataSectors2);
        int dataSectors3 [NumDirect + 2];
        synchDisk->ReadSector(dataSectors[NumDirect - 1], (char*)dataSectors3);

        printf("FileHeader contents.  File size: %d.  File blocks:\n", numBytes);
        for (i = 0; i < NumDirect; i++)
            printf("%d ", dataSectors[i]);
        printf("\nFile contents:\n");

        for (i = k = 0; i < NumDirect - 2; i++) {
            counter++;
            synchDisk->ReadSector(dataSectors[i], data);
            for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
                if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
                    printf("%c", data[j]);
                else
                    printf("\\%x", (unsigned char)data[j]);
            }
            printf("\n"); 
        }

        for (i = k = 0; i < NumDirect + 2; i++) {
            counter++;
            synchDisk->ReadSector(dataSectors2[i], data);
            for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
                if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
                    printf("%c", data[j]);
                else
                    printf("\\%x", (unsigned char)data[j]);
            }
            printf("\n"); 
        }

        for (int l = 0; counter < numSectors && k < numBytes; l++){
            //printf("counter: %i~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n", counter);
            int sectors[NumDirect + 2];
            synchDisk->ReadSector(dataSectors3[l], (char*)sectors);
            
            for (i = 0; i < NumDirect + 2 && counter < numSectors; i++) {
                counter++;
                //printf("counter: %i~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n", counter);
                //printf("num sectors needed: %i\n", numSectors);
                synchDisk->ReadSector(sectors[i], data);
                for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
                    if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
                        printf("%c", data[j]);
                    else
                        printf("\\%x", (unsigned char)data[j]);
                }
                printf("\n"); 
            }
        }
        printf("counter: %i~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n", counter);
        printf("k: %i\n", k);
    }

    delete [] data;
}
