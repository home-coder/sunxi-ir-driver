/* Copyright (C) 
 * 2014 - Allwinnertech.com
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * 
 */

/**
 * @file multi-ir.c
 * @brief : multi ir user space config tool
 * @author Allwinnertech.com
 * @version v0.1
 * @date 2014-12-06
 */

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <utils/Log.h>

#include "multi_ir.h"

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "multi_ir"

#define RESERVE_CODE	0
#define RESERVE_NAME	"!RESERVE"

#undef DEBUG
#define IR_DEV_PATH				"/dev/sunxi-multi-ir"
#define KEY_LAYOUT_PATH			"/system/usr/keylayout"
#define DEFAULT_KEYLAYOUT_FILE	"sunxi-ir.kl"

/*
 * A valid customer ir key layout file *MUST* namd by 'customer_ir_xxxx.kl',
 * which 'xxxx' is the identity(ir address code, in hex),
 * eg. customer_ir_9f00.kl
*/
#define FILENAME_PREFIX			"customer_ir_"
#define FILENAME_EXTENSION		".kl"

struct keymap_t {
	int keycode;
	char name[MAX_NAME_LEN];
};

int mapping_table_cnt = 0;
struct keymap_t default_keys[KEYCODE_CNT];
struct keymap_t customer_keys[KEYCODE_CNT];
struct mapping_table_t mapping_table;

void keymap_init(struct keymap_t *map)
{
	int i;
	for (i=0; i<KEYCODE_CNT; i++, map++) {
		map->keycode = RESERVE_CODE;
		strcpy(map->name, RESERVE_NAME);
	}
}

#ifdef DEBUG
void dump_keymap(struct keymap_t *map)
{
	int i;

	for (i=0; i<KEYCODE_CNT; i++, map++) {
		if (!strlen(map->name)) continue;
		printf("key\t%d\t%s\n", map->keycode, map->name);
	}
}

void dump_mapping_table(struct mapping_table_t *table, struct keymap_t *def)
{
	int i;

	printf("+ identity: 0x%04x\n", table->identity);
	for (i=0; i<KEYCODE_CNT; i++) {
		printf("\t[%3d] --> { %3d, %-20s }\n", i, table->value[i],
					(def + table->value[i])->name);
	}
}
#endif

/**
 * @brief: create a keymap form keylayout file(*.kl)
 * @param: path - path to the target file
 * @param: map - store the result
 * @return:
 */
int create_keymap_from_file(const char *path, struct keymap_t *map)
{
	int ret = 0;
	FILE *kl_fd = NULL;
	char buf[1024], lable[32], name[MAX_NAME_LEN];
	int keycode;
	char *p;
	struct keymap_t *des;

	kl_fd = fopen(path, "r");
	if (!kl_fd) {
		fprintf(stderr, "open '%s' fail, %s\n", path, strerror(errno));
		return -1;
	}

	while (fgets(buf, 1024, kl_fd)) {
		p = buf;
		while (*p==' ') p++;
		if (*p=='#') continue;

		if (sscanf(buf, "%s %d %s", lable, &keycode, name)!=3) continue;
		if (strcmp(lable, "key")!=0 || keycode < KEYCODE_MIN || keycode > KEYCODE_MAX) continue;

		des = (struct keymap_t *)(map + keycode);
		des->keycode = keycode;
		strcpy(des->name, name);
	}

	if (!feof(kl_fd)) {
		fprintf(stderr, "reading '%s' error, %s\n", path, strerror(errno));
		ret = -1;
	}

	fclose(kl_fd);
	return ret;
}

/**
 * @brief: make a mapping from src to des, the result is store at @table,
 *         the mapping will set to ir driver through ioctl syscall.
 * @param: src
 * @param: des
 * @param: table
 */
void generate_mapping_table(struct keymap_t* src, struct keymap_t *des, struct mapping_table_t *table)
{
	int i, j;
	struct keymap_t *p;

	memset(table, 0, sizeof(struct mapping_table_t));
	for (i=0; i<KEYCODE_CNT; i++, des++) {

		p = src;
		for (j=0; j<KEYCODE_CNT; j++, p++) {
			if (p->keycode==RESERVE_CODE) continue;
			if (strcmp(des->name, p->name)==0) {
				table->value[des->keycode] = p->keycode;

				if (table->powerkey==0 && strcmp(des->name, "POWER")==0) {
					table->powerkey = des->keycode;
				}

				break;
			}
		}
	}
}

/**
 * @brief: filter out the product model.
 * @param: name
 * @return: -1 means something error, else the product model.
 */
#define CMDLINE "/proc/cmdline"
int get_product_model(char model[])
{
    FILE * fp;
    char buf[1024] = {0};
    char *str = NULL;

    fp = fopen(CMDLINE, "r");
    if (NULL == fp)
    {
        printf("open %s failed!\n",CMDLINE);
        return -1;
    }
    if(!fread(buf, 1, 1024, fp))
    {
        printf("read file filed!\n");
        return -1;
    }
    
    if((str = strstr(buf, "inside_model")) == NULL)
    {
        printf("get model failed!\n");
        return -1;
    }

    //printf("str= %s\n", str);
    str = str + strlen("inside_model=");
    while((*model++ = *str++) != ' ');

    *(model - 1) = '\0';
    fclose(fp);
    return 0;
}
/**
 * @brief: filter out the valid kaylaout file.
 * @param: name
 * @return: -1 means something error, else the identity.
 */
int kl_filename_verify(const char *name, char model[])
{
	char *p;
	int i, identity;

	/* filename prefix filter */
	if (strncmp(name, FILENAME_PREFIX, strlen(FILENAME_PREFIX)))
		return -1;

    /* product model filter 
     */
    if (strlen(model) != 0)
    {  
        // too many product, support default 7 customer kl
        if (!strncmp(model, "CanC", 4) || !strncmp(model, "AMOI_B", 6) || !strncmp(model, "EARISE_K", 8))
        {
	        p = (char *)name + strlen(FILENAME_PREFIX) + 4;
	        if (strncmp(p, FILENAME_EXTENSION, strlen(FILENAME_EXTENSION)))
	    	    return -1;
            //printf("product model= %s, file name = %s \n",model,name);
        }
        else
        {
            p = (char *)name + strlen(FILENAME_PREFIX) + 5;
            //printf("get %s product model %s, %s, %d\n",name,model,p,strlen(model));
            if (!strncmp(p, model, strlen(model)))
            {
    	        /* model matched, filename extensiion filter */
    	        p = (char *)name + strlen(name) - 3;
	            if (strncmp(p, FILENAME_EXTENSION, strlen(FILENAME_EXTENSION)))
	                return -1;
            }
            else
            {    
	            /* model is not match, use default 4cb3 customer kl */
	            p = (char *)name + strlen(FILENAME_PREFIX);
                if (strncmp(p, "4cb3", 4) && strncmp(p, "fe01", 4))
                    return -1;
	            p = (char *)name + strlen(FILENAME_PREFIX) + 4;
	            if (strncmp(p, FILENAME_EXTENSION, strlen(FILENAME_EXTENSION)))
	    	        return -1;
            }
        }
    }
    else
    {    
	    /* filename extensiion filter */
	    p = (char *)name + strlen(FILENAME_PREFIX) + 4;
	    if (strncmp(p, FILENAME_EXTENSION, strlen(FILENAME_EXTENSION)))
	    	return -1;
    }

	/* identity filter, 4 hexadecimal number */
	p = (char *)name + strlen(FILENAME_PREFIX);
	for (i=0; i<4; i++) {
		if (isxdigit(*(p+i))==0)
			return -1;
	}
	
	ALOGD("----%s------------------\n", p);
	identity =  strtol(p, &p, 16);
	ALOGD("----%s--------------0x%x----\n", p, identity);

	return identity;
}

int main(int argc, char **argv)
{
	int dev_fd, ret = 0;
	char kl_path[512] = {0};
	DIR *dir;
	struct dirent *dirent;
	int identity;

	dev_fd = open(IR_DEV_PATH, O_RDWR);
	if (dev_fd<0) {
		fprintf(stderr, "Open device '%s' error\n", IR_DEV_PATH);
		exit(-1);
	}

	if (ioctl(dev_fd, MULTI_IR_IOC_REQ_MAP, &mapping_table_cnt)<0) {
		fprintf(stderr, "ioctl 'MULTI_IR_IOC_REQ_MAP' error\n");
		ret = -1;
		goto err_ioctl;
	}

	if (mapping_table_cnt<=0) {
		fprintf(stderr, "sunxi ir driver is to old to support multi mode\n ");
		ret = -1;
		goto err_ioctl;
	}
	ALOGD("the max mapping_table support by driver: %d\n", mapping_table_cnt);

	/* create default key map */
	keymap_init(default_keys);
	sprintf(kl_path, "%s/%s", KEY_LAYOUT_PATH, DEFAULT_KEYLAYOUT_FILE);
	create_keymap_from_file(kl_path, default_keys);
	#ifdef DEBUG
	dump_keymap(default_keys);
	#endif

	dir = opendir(KEY_LAYOUT_PATH);
	if (!dir) {
		fprintf(stderr, "Open directory '%s' error, %s\n",
			KEY_LAYOUT_PATH, strerror(errno));
		exit(-1);
	}

    char model[32]={0};
    get_product_model(model);
	while ((dirent=readdir(dir))!=NULL) {
		identity = kl_filename_verify(dirent->d_name, model);
		if (identity!=-1) {
			ALOGD("config filename %s\n", dirent->d_name);

			sprintf(kl_path, "%s/%s", KEY_LAYOUT_PATH, dirent->d_name);
			memset(customer_keys, 0, sizeof(struct keymap_t)*(KEYCODE_CNT));
			create_keymap_from_file(kl_path, customer_keys);
			generate_mapping_table(default_keys, customer_keys, &mapping_table);
			mapping_table.identity = identity;
			#ifdef DEBUG
			dump_mapping_table(&mapping_table, default_keys);
			#endif

			if (ioctl(dev_fd, MULTI_IR_IOC_SET_MAP, &mapping_table)<0) {
				fprintf(stderr, "ioctl 'MULTI_IR_IOC_SET_MAP' error, identity: 0x%04x\n", identity);
				ret = -1;
				break;
			}

		}
	}

	closedir(dir);
err_ioctl:
	close(dev_fd);
	return ret;
}
