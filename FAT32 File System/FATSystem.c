#include <stdlib.h>
#include <stdio.h>
#include <string.h>

//Structs to aid with file system
typedef struct File {
	char name[100];
	int size;
	int first_cluster;
	int mode;
	int attributes;
	size_t offset;
} File;

typedef struct Directory{
	File files[100];
	int filecount;
};

//Global variables
FILE* imgf;
size_t bytes_per_sector;
size_t sectors_per_cluster;
size_t reserved_sector_count;
size_t number_of_FATs;
size_t total_sectors;
size_t FATsize;
size_t root_cluster;
size_t current_cluster = 0;
struct Directory working_directory;
size_t first_sector;
struct File openfiles[100];
int openfilecount = 0;

//Function declarations
static char* UserCmd();
void GetBSInfo();
int BSInfoDriver(char* array, int bytes);
void FileSize(char* fn);
struct Directory getDirectory(int cluster);
struct File* DirectorySearch(char* fn);
size_t FirstCluster(int cluster);
size_t SectorLocation(size_t sector);
size_t DirectoryDriver(size_t offset, size_t size);
void ListDirectory(char* dn);
void ChangeDirectory(char* dn);
void CreateFile(char* fn);
size_t NextCluster();
void OpenFile(char* fn, char* md);
void CloseFile(char* fn);
void lseek(char* fn, size_t offset);
void ReadFile(char* fn, size_t sz);
void WriteFile(char* fn, size_t sz, char* msg);
void RemoveFile(char* fn);
void RemoveDirectory(char* fn);
void SetDirectory();
void Write(size_t val, size_t offset);
void MakeDirectory(char* fn);

int main(){
	imgf = fopen("fat32.img", "rb+");
	if(!imgf){
		fputs("ERROR: fat32.img does not exist.\n", stderr);
		exit(1);
	}
	char* cmd;
	cmd = (char*) malloc(sizeof(char)*100);
	int exit = 0;
	GetBSInfo();
	while(exit == 0){
		cmd = UserCmd();
		cmd[strlen(cmd) - 1] = '\0';
		if(strncmp(cmd, "Exit", 4) == 0 || strncmp(cmd, "exit", 4) == 0){
			fclose(imgf);
			exit = 1;
			return 0;
			break;
		}
		else if(strncmp(cmd, "Info", 4) == 0 || strncmp(cmd, "info", 4) == 0){
			printf("Bytes per sector: %zu\n", bytes_per_sector);
			printf("Sectors per cluster: %zu\n", sectors_per_cluster); 
			printf("Reserved sector count: %zu\n", reserved_sector_count); 
			printf("Number of FATs: %zu\n", number_of_FATs); 
			printf("Total sectors: %zu\n", total_sectors); 
			printf("FATsize: %zu\n", FATsize); 
			printf("Root cluster: %zu\n", root_cluster);
		}
		else if(strncmp(cmd, "size ", 5) == 0){
			if(strlen(cmd) >= 6){
				cmd+=5;
				FileSize(cmd);
			}
			else{
				printf("USAGE: size FILENAME\n");
			}
		}
		else if(strncmp(cmd, "ls ", 3) == 0){
			if(strlen(cmd) >= 4){
				cmd+=3;
				ListDirectory(cmd);
			}
			else{
				printf("USAGE: ls DIRNAME\n");
			}
		}
		else if(strncmp(cmd, "cd ", 3) == 0){
			if(strlen(cmd) >= 4){
				cmd+=3;
				ChangeDirectory(cmd);
			}
			else{
				printf("USAGE: cd DIRNAME\n");
			}
		}
		else if(strncmp(cmd, "creat ", 6) == 0){
			if(strlen(cmd) >= 7){
				cmd+=6;
				CreateFile(cmd);
			}
			else{
				printf("USAGE: creat FILENAME\n");
			}
		}
		else if(strncmp(cmd, "mkdir ", 6) == 0){
			if(strlen(cmd) >= 7){
				cmd+=6;
				MakeDirectory(cmd);
			}
			else{
				printf("USAGE: mkdir DIRNAME\n");
			}
		}
		else if(strncmp(cmd, "open ", 5) == 0){
			if(strlen(cmd) >= 6){
				char name[20];
				char mode[4];
				sscanf(cmd, "%*s %s %s", name, mode);
				if(strpbrk(mode, "wr") == NULL){
					printf("USAGE: open FILENAME MODE\n");
				}else {
				OpenFile(name, mode);
				}
			}
			else{
				printf("ERROR: File name and mode required\n");
			}
		}
		else if(strncmp(cmd, "close ", 6) == 0){
			if(strlen(cmd) >= 7){
				cmd+=6;
				CloseFile(cmd);
			}
			else{
				printf("USAGE: close FILENAME");
			}
		}
		else if(strncmp(cmd, "lseek ", 6) == 0){
			if(strlen(cmd) >= 7){
				char name[20];
				int offset;
				sscanf(cmd, "%*s %s %d", name, &offset);
				lseek(name, offset);
			}
			else{
				printf("USAGE: lseek FILENAME OFFSET\n");
			}
		}
		else if(strncmp(cmd, "read ", 5) == 0){
			if(strlen(cmd) >= 6){
				char name[20];
				size_t filesize;
				sscanf(cmd, "%*s %s %d", name, &filesize);
				ReadFile(name, filesize);
			}
			else{
				printf("USAGE: read FILENAME SIZE\n");
			}
		}
		else if(strncmp(cmd, "write ", 6) == 0){
			if(strlen(cmd) >= 7){
				char name[20];
				size_t sz;
				char msg[1024];
				sscanf(cmd, "%*s %s %d %s", name, &sz, msg);
				WriteFile(name, sz, msg);
			}
			else{
				printf("USAGE: write FILENAME SIZE ""STRING""\n");
			}
		}
		else if(strncmp(cmd, "rm ", 3) == 0){
			if(strlen(cmd) >= 4){
				cmd+=3;
				RemoveFile(cmd);
			}
			else{
				printf("USAGE: rm FILENAME\n");
			}
		}
		else if(strncmp(cmd, "rmdir ", 6) == 0){
			if(strlen(cmd) >= 7){
				cmd+=6;
				RemoveFile(cmd);
			}
			else{
				printf("USAGE: rmdir DIRNAME\n");
			}
		}
	}
} //end of main


//Function Definitions 

static char* UserCmd(){
	printf("%s@%s:%s>", getenv("USER"), getenv("MACHINE"), getenv("PWD"));
	static char cmd[100];
	fgets(cmd, 100, stdin);
	return cmd;
}

void GetBSInfo(){
	short bytes[16];
	char SecPerClus[1];
	fseek(imgf, 11, SEEK_SET);
        fread(bytes, sizeof(char), 2, imgf);
        bytes_per_sector = bytes[0];
	fseek(imgf, 13, SEEK_SET);
        fread(bytes, sizeof(char), 1, imgf);
        sectors_per_cluster = BSInfoDriver(SecPerClus, 4);
	fseek(imgf, 14, SEEK_SET);
	fread(bytes, sizeof(char), 2, imgf);
	reserved_sector_count = BSInfoDriver(bytes, 2);
	fseek(imgf, 16, SEEK_SET);
	fread(bytes, sizeof(char), 1, imgf);
	number_of_FATs = BSInfoDriver(bytes, 1);
	fseek(imgf, 32, SEEK_SET);
	fread(bytes, sizeof(char), 4, imgf);
	total_sectors = BSInfoDriver(bytes, 4);
	fseek(imgf, 36, SEEK_SET);
	fread(bytes, sizeof(char), 4, imgf);
	FATsize = BSInfoDriver(bytes, 4);
	fseek(imgf, 44, SEEK_SET);
	fread(bytes, sizeof(char), 4, imgf);
	root_cluster = BSInfoDriver(bytes, 4);
	first_sector = number_of_FATs * FATsize + reserved_sector_count;
}

int BSInfoDriver(char* array, int bytes){
	size_t retval = 0;
	int i = 0;
	while(i < bytes){
		retval += (unsigned int)(array[i])<<(i*8);
		i++;
	}
	return retval;
}

//Part 3 (size)
void FileSize(char* fn){
	File* fp;
	SetDirectory();
	fp = DirectorySearch(fn);
	if(fp == NULL){
		printf("ERROR: '%s' not found\n", fn);
	}
	else{
		printf("'%s' - %d\n", fn, fp->size);
	}
}

struct Directory getDirectory(int cluster){
	struct Directory d;
	d.filecount = 0;
	int filecounter = 0;
	size_t first = FirstCluster(cluster);
	size_t location = SectorLocation(first);
	int i;
	size_t limit = bytes_per_sector * sectors_per_cluster;
	for(i = 0; i < limit; i+=32){
		struct File current;
		char temp = DirectoryDriver(location+i+11, 1);
		if(temp == 15){
			continue;
		}
		int j;
		char dirname[12];
		for(j = 0; j < 11; j++){
			char dirchar = DirectoryDriver(location+i+j, 1);
			if(dirchar == 0){
				dirname[j] = ' ';
			}
			else{
				dirname[j] = dirchar;
			}
		}
		if(dirname[0] == ' '){
			continue;
		}
		dirname[11] = '\0';
		size_t size = DirectoryDriver(location+i+28, 4);
		size_t low = DirectoryDriver(location+i+26, 2);
		size_t high = DirectoryDriver(location+i+20, 2);
		high << 16;
		size_t hilo = high | low;
		current.size = size;
		current.attributes = temp;
		current.first_cluster = hilo;
		strcpy(current.name, dirname);
		d.files[d.filecount] = current;
		d.filecount++;
	}
	return d;
}

size_t FirstCluster(int cluster){
	size_t first;
	first = first_sector + ((cluster - 2) * sectors_per_cluster);
	return first;
}

size_t SectorLocation(size_t sector){
	int retval = sector * bytes_per_sector * sectors_per_cluster;
	return retval;
}

size_t DirectoryDriver(size_t offset, size_t size){
	int counter = size;
	counter--;
	size_t retval;
	while(counter > 0){
		fseek(imgf, offset, SEEK_SET);
		fseek(imgf, size, SEEK_CUR);
		size_t  datatemp = fgetc(imgf);
		retval = retval | datatemp;
		counter--;
		if(size != 0){
			retval = retval << 8;
		}
	}
	return retval;
}

struct File* DirectorySearch(char* fn){
	struct File* f;
	int i;
	for(i = 0; i < working_directory.filecount; i++){
		f = &working_directory.files[i];
		if(strcmp(fn, f->name) == 0){
			return f;
		}
	}
	return NULL;
}

void SetDirectory(){
	if(current_cluster == 0){
		current_cluster = root_cluster;
		working_directory = getDirectory(current_cluster);
	}
}

//Part 4 (ls)
void ListDirectory(char* dn){
	SetDirectory();
	if(working_directory.filecount > 0){
		if(current_cluster == root_cluster && strcmp(dn, ".") == 0){
			if(working_directory.filecount == 0){
				printf("Directory is empty\n");
				return;
			}
			int i;
			for(i = 0; i < working_directory.filecount; i++){
				struct File* x = &working_directory.files[i];
				printf("%s\n", x->name);
			}
			return;
		}
		struct File* nd = DirectorySearch(dn);
		if(nd == NULL){
			printf("ERROR: '%s' not found\n", dn);
			return;
		}
		struct Directory temp = getDirectory(nd->first_cluster);
		int i;
		for(i = 0; i < temp.filecount; i++){
			struct File* x = &temp.files[i];
			printf("%s\n", x->name);
		}
	}
	else{
		printf("Directory is empty\n");
		return;
	}
}

// Part 5 (cd)
void ChangeDirectory(char* dn){
	SetDirectory();
	struct File* nd = DirectorySearch(dn);
	if(nd == NULL){
		printf("ERROR: '%s' not found\n", dn);
		return;
	}
	current_cluster = nd->first_cluster;
	working_directory = getDirectory(current_cluster);
}

// Part 6 (creat)
void CreateFile(char* fn){
	SetDirectory();
	int fileExists = 0;	//filname is already in the directory
	int i = 0;
	for(i = 0; i < working_directory.filecount; i++){
		if(strcmp(working_directory.files[i].name, fn) == 0){
			printf("ERROR: '%s' already exists\n", fn);
			fileExists = 1;
		}
	}

	if(fileExists == 0)
	{
		size_t fc = NextCluster();
		if(fc == 0xFFFFFFFF){
			return;
		}
		size_t offset1 = ((root_cluster * 4) / bytes_per_sector) + reserved_sector_count + (fc * 4);
		size_t offset2 = ((root_cluster * 4) / bytes_per_sector) + reserved_sector_count + (fc * 4)
		+ (FATsize * bytes_per_sector);
		Write(0x0FFFFFF8, offset1);
		Write(0x0FFFFFF8, offset2);
		strcpy(working_directory.files[working_directory.filecount].name, fn);
		working_directory.files[working_directory.filecount].size = 0;
		working_directory.files[working_directory.filecount].first_cluster = fc;
		working_directory.files[working_directory.filecount].offset = 0;
		working_directory.files[working_directory.filecount].attributes = 0x00;
		working_directory.filecount++;
		printf("%s created: 0 bytes used, 0 bytes remaining\n", fn);
	}
}

size_t NextCluster(){
	size_t sector = ((root_cluster * 4) / bytes_per_sector) + reserved_sector_count;
	size_t end = (FATsize * bytes_per_sector) + sector;
	size_t bytes[4];
	size_t cluster;
	size_t cluster_count = 0;
	do{
		fseek(imgf, sector, SEEK_SET);
		fread(bytes, sizeof(char), 4, imgf);
		cluster = BSInfoDriver(bytes, 4);
		sector+=4;
		cluster_count++;
	}while(cluster != 0 || sector >= end);

	if(sector == end){
		printf("ERROR: fat32.img is full\n");
		return 0xFFFFFFFF;
	}
	cluster_count--;
	return cluster_count;
}

void Write(size_t val, size_t offset){
	fseek(imgf, offset, SEEK_SET);
	static unsigned char retval[4];
	memset(retval, 0, 4);
	int i = 0;
	size_t valt = val;
	size_t hex = 0x000000FF;
	for(i = 0; i < 4; i++){
		retval[i] = valt&hex;
		valt = valt >> 8;
	}
	unsigned char* arr = retval;
	for(i = 0; i < 4; i++){
		fputc(arr[i], imgf);
	}
}

// Part 7 (mkdir)
void MakeDirectory(char* fn){       
        SetDirectory();
        int i = 0;
        for(i = 0; i < working_directory.filecount; i++){
                if(strcmp(working_directory.files[i].name, fn) == 0){
                        printf("ERROR: '%s' already exists\n", fn);
						return;
                }
        }
        size_t fc = NextCluster();
        if(fc == 0xFFFFFFFF){
                return;           
        }
        size_t offset1 = ((root_cluster * 4) / bytes_per_sector) + reserved_sector_count + (fc * 4);
        size_t offset2 = ((root_cluster * 4) / bytes_per_sector) + reserved_sector_count + (fc * 4)
		 + (FATsize * bytes_per_sector);
        Write(0x0FFFFFF8, offset1);
        Write(0x0FFFFFF8, offset2);
        strcpy(working_directory.files[working_directory.filecount].name, fn);
        working_directory.files[working_directory.filecount].size = 0;
        working_directory.files[working_directory.filecount].first_cluster = fc;
		working_directory.files[working_directory.filecount].attributes = 0x10;
        working_directory.filecount++;
		printf("%s created: 0 bytes used, 0 bytes remaining\n", fn);
}            

// Part 9 (open)
void OpenFile(char* fn, char* md){
	SetDirectory();
	int mode = -1;
	if(strcmp(md, "r") == 0){
		mode = 1;
	}
	else if(strcmp(md, "w") == 0){
		mode = 2;
	}
	else if(strcmp(md, "wr") == 0){
		mode = 3;
	}
	else if(strcmp(md, "rw") == 0){
		mode = 3;
	}
	int i;
	int checker = 0;
	struct File* fc = DirectorySearch(fn);
	if(fc == NULL){
		printf("ERROR: '%s' not found\n", fn);
		return;
	}
	struct File* f;
	for(i = 0; i < openfilecount; i++){
		f = &openfiles[i];
		if(strcmp(f->name, fn) == 0){
			printf("ERROR: '%s' already open\n", fn);
			return;
		}
	}
	struct File* new;
	new = &openfiles[openfilecount];
	new->first_cluster = fc->first_cluster;
	new->attributes = fc->attributes;
	new->mode = mode;
	strcpy(new->name, fc->name);
	openfilecount++;
}

// Part 10 (close)
void CloseFile(char* fn){
	struct  File* f;
	int checker = -1;
	int i;
	for(i = 0; i < openfilecount; i++){
		f = &openfiles[i];
		if(strcmp(fn, f->name) == 0){
			checker = i;
		}
	}
	if(checker == -1){
		printf("ERROR: '%s' not open\n", fn);
		return;
	}
	f = &openfiles[checker];
	f->name[0] = '\0';
	f->size = 0;
	f->attributes = 0;
	f->mode = 0;
	openfilecount--;
	printf("Closed file %s\n", fn);
}

//Part 11 (lseek)
void lseek(char* fn, size_t offset){
	SetDirectory();
	struct File* f = DirectorySearch(fn);
	if(f == NULL){
		printf("ERROR: '%s' not found\n", fn);
		return;
	}
	if(offset > f->size){
		printf("ERROR: Offset larger than file\n");
		return;
	}
	f->offset = offset;
}

//Part 12 (read)
void ReadFile(char* fn, size_t sz){
	SetDirectory();
	struct File* f = DirectorySearch(fn);
	if(f == NULL){
		printf("ERROR: '%s' not found\n", fn);
		return;
	}
	struct File* temp;
	int checker = -1;
	int i;
	for(i = 0; i < openfilecount; i++){
		temp = &openfiles[i];
		if(strcmp(fn, temp->name) == 0){
			checker = i;
		}
	}
	if(checker == -1){
		printf("ERROR: '%s' not open\n", fn);
		return;
	}
	if(openfiles[checker].mode == 2){
		printf("ERROR: '%s' not opened in a readable mode\n", fn);
	}
	size_t fc = f->first_cluster;
	fc = fc - 2;
	fc = (fc * sectors_per_cluster) + reserved_sector_count + (number_of_FATs * FATsize);
	fc = fc * bytes_per_sector * sectors_per_cluster;
	fseek(imgf, f->offset + fc, SEEK_SET);
	char bytes[sz+1];
	fread(bytes, 1, sz, imgf);
	printf("%s\n", bytes);
}

// Part 13 (write)
void WriteFile(char* fn, size_t sz, char* msg){
	SetDirectory();
	struct File* f = DirectorySearch(fn);
	if(f == NULL){
		printf("ERROR: '%s' not found\n", fn);
		return;
	}
	int i;
	int checker = -1;
	struct File* temp;
	for(i = 0; i < openfilecount; i++){
		temp = &openfiles[i];
		if(strcmp(fn, temp->name) == 0){
			checker = i;
			break;
		}
	}
	if(checker == -1){
		printf("ERROR: '%s' not open\n", fn);
		return;
	}
	if(openfiles[checker].mode == 0){
		printf("ERROR: '%s' not open in a writable mode\n", fn);
		return;
	}
	size_t start = temp->first_cluster;
	start = start - 2;
	start = (start * sectors_per_cluster) + reserved_sector_count + (number_of_FATs * FATsize);
        start = start * bytes_per_sector * sectors_per_cluster;
	fseek(imgf, start + temp->offset, SEEK_SET);
	fwrite(msg, 1, sz, imgf);
}

//Part 14 (rm)
void RemoveFile(char* fn){
	struct Directory d = getDirectory(current_cluster);
	int i;
	int checker = -1;
	for(i = 0; i < d.filecount; i++){
		if(strcmp(d.files[i].name, fn) == 0){
			if(d.files[i].attributes&0x10){
				printf("ERROR: '%s' is a directory\n", fn);
			}
			else{
				checker = i;
				break;
			}
		}
	}
	if(checker == -1){
		printf("ERROR: '%s' not found\n", fn);
		return;
	}
	size_t x = 0xE5;
	fseek(imgf, d.files[checker].offset, SEEK_SET);
	fputc(x, imgf);
	size_t fcn = d.files[checker].first_cluster;
	size_t clusters[65536];
	i = 0;
	int fcn2 = fcn;
	size_t y;
	while(fcn2 < 0x0FFFFFF8){
		clusters[i] = fcn2;
		char arr[4];
		y = (((root_cluster * 4) / bytes_per_sector) + reserved_sector_count) * bytes_per_sector;
		size_t y2 = y + (fcn2 * 4);
		fseek(imgf, y2, SEEK_SET);
		fread(arr, sizeof(char), 4, imgf);
		fcn2 = BSInfoDriver(arr, 4);
		i++;
	}
	i = i - 1;
	while(i >= 0){
		int fs = (number_of_FATs * FATsize) + reserved_sector_count;
		int fsc = ((clusters[i] - 2) * sectors_per_cluster) + fs;
		size_t sec = bytes_per_sector * fsc;
		size_t bytes = bytes_per_sector * sectors_per_cluster;
		fseek(imgf, sec, SEEK_SET);
		int j = 0;
		while(j < bytes){
			fputc('\0', imgf);
			j++;
		}
		size_t off1 = (clusters[i] * 4) + y;
		size_t off2 = y + (FATsize * bytes_per_sector) + (clusters[i] * 4);
		fseek(imgf, off1, SEEK_SET);
		char arr2[4];
		memset(arr2, 0, 4);
		j = 0;
		while(j < off1){
			arr2[j] = 0&0x000000FF;
			j++;
		}
		j = 0;
		while(j < 4){
			fputc(arr2[j], imgf);
			j++;
		}
		char arr3[4];
		memset(arr3, 0, 4);
		j = 0;
		while(j < off2){
			arr3[j] = 0&0x000000FF;
			j++;
		}
		j = 0;
		while(j < 4){
			fputc(arr2[j], imgf);
			j++;
		}
		i++;
	}
}

// Extra Credit (rmdir)
void RemoveDirectory(char* fn){
        struct Directory d = getDirectory(current_cluster);
        int i;
        int checker = -1;
        for(i = 0; i < d.filecount; i++){
                if(strcmp(d.files[i].name, fn) == 0){
                        if(d.files[i].attributes&0x10){
                                printf("ERROR: '%s' not a directory\n", fn);
                        }
                        else{
                                checker = i;
                                break;
                        }
                }
        }
        if(checker == -1){
                printf("ERROR: '%s' not found\n", fn);
                return;       
        }
	ChangeDirectory(fn);
	int check;
	d = getDirectory(current_cluster);
	if(d.filecount > 1){
		printf("ERROR: '%s' not empty\n", fn);
		ChangeDirectory("..");
		return;
	}
	ChangeDirectory("..");
	d = getDirectory(current_cluster);
	size_t x = 0xE5;
        fseek(imgf, d.files[checker].offset, SEEK_SET);
        fputc(x, imgf);
        size_t fcn = d.files[checker].first_cluster;
        size_t clusters[65536];
        i = 0;
        int fcn2 = fcn;
        size_t y;
        while(fcn2 < 0x0FFFFFF8){
                clusters[i] = fcn2;
                char arr[4];
                y = (((root_cluster * 4) / bytes_per_sector) + reserved_sector_count) 
				* bytes_per_sector;
                size_t y2 = y + (fcn2 * 4);
                fseek(imgf, y2, SEEK_SET);
                fread(arr, sizeof(char), 4, imgf);
                fcn2 = BSInfoDriver(arr, 4);
                i++;
        }
        i = i - 1;
        while(i >= 0){
                int fs = (number_of_FATs * FATsize) + reserved_sector_count;
                int fsc = ((clusters[i] - 2) * sectors_per_cluster) + fs;
                size_t sec = bytes_per_sector * fsc;
                size_t bytes = bytes_per_sector * sectors_per_cluster;
                fseek(imgf, sec, SEEK_SET);
                int j = 0;
                while(j < bytes){
                        fputc('\0', imgf);
                        j++;
                }
                size_t off1 = (clusters[i] * 4) + y;
                size_t off2 = y + (FATsize * bytes_per_sector) + (clusters[i] * 4);
                fseek(imgf, off1, SEEK_SET);
                char arr2[4];
                memset(arr2, 0, 4);
                j = 0;
                while(j < off1){
                        arr2[j] = 0&0x000000FF;
                        j++;
                }
                j = 0;
                while(j < 4){
                        fputc(arr2[j], imgf);
                        j++;
                }
                char arr3[4];
                memset(arr3, 0, 4);
		j = 0;
                while(j < off2){
                        arr3[j] = 0&0x000000FF;
                        j++;
                }      
                j = 0;
                while(j < 4){
                        fputc(arr2[j], imgf);          
                        j++;
                }
                i++;           
        }     
}
