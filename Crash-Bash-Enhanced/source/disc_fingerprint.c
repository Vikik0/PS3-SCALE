#include "disc_fingerprint.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define CB_FP_PATH_MAX 512
#define CB_FP_LINE_MAX 1024
#define CB_FNV_OFFSET 14695981039346656037ULL
#define CB_FNV_PRIME 1099511628211ULL

static uint64_t hash_bytes(uint64_t hash, const void *data, size_t length)
{
    const unsigned char *bytes = (const unsigned char *)data;
    size_t i;
    for (i = 0; i < length; i++) { hash ^= bytes[i]; hash *= CB_FNV_PRIME; }
    return hash;
}
static uint64_t hash_text(uint64_t hash, const char *text)
{
    static const unsigned char separator = 0;
    hash = hash_bytes(hash, text, strlen(text));
    return hash_bytes(hash, &separator, 1);
}
static int resolve_track_path(const char *cue_path, const char *filename, char *out, size_t cap)
{
    const char *slash;
    size_t dir_len;
    int n;
    if (!cue_path || !filename || !filename[0] || !out || cap == 0) return -1;
    if (filename[0] == '/') {
        n = snprintf(out, cap, "%s", filename);
        return n >= 0 && (size_t)n < cap ? 0 : -1;
    }
    slash = strrchr(cue_path, '/');
    if (!slash) {
        n = snprintf(out, cap, "%s", filename);
        return n >= 0 && (size_t)n < cap ? 0 : -1;
    }
    dir_len = (size_t)(slash - cue_path) + 1;
    if (dir_len + strlen(filename) + 1 > cap) return -1;
    memcpy(out, cue_path, dir_len); out[dir_len] = '\0';
    memcpy(out + dir_len, filename, strlen(filename) + 1);
    return 0;
}
static int hash_track(uint64_t *hash, const char *cue_path, const char *filename, const char *type)
{
    char path[CB_FP_PATH_MAX];
    unsigned char buffer[65536], size_bytes[8];
    FILE *file;
    size_t count;
    uint64_t size = 0;
    int read_error, close_error, i;
    if (!hash || resolve_track_path(cue_path, filename, path, sizeof(path)) != 0) return -1;
    file = fopen(path, "rb");
    if (!file) return -1;
    *hash = hash_text(*hash, "TRACK_FILE");
    *hash = hash_text(*hash, type);
    while ((count = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        *hash = hash_bytes(*hash, buffer, count);
        size += (uint64_t)count;
    }
    read_error = ferror(file); close_error = fclose(file);
    if (read_error || close_error || size == 0) return -1;
    for (i = 0; i < 8; i++) size_bytes[7-i] = (unsigned char)((size >> (i*8)) & 0xffU);
    *hash = hash_bytes(*hash, size_bytes, sizeof(size_bytes));
    return 0;
}
static int parse_file(uint64_t *hash, const char *cue_path, char *line)
{
    char filename[CB_FP_PATH_MAX], type[32], *p = line, *end;
    size_t name_len = 0, type_len, i;
    int quote;
    while (*p && isspace((unsigned char)*p)) p++;
    if (toupper((unsigned char)p[0])!='F' || toupper((unsigned char)p[1])!='I' ||
        toupper((unsigned char)p[2])!='L' || toupper((unsigned char)p[3])!='E' ||
        !isspace((unsigned char)p[4])) return 0;
    p += 4; while (*p && isspace((unsigned char)*p)) p++;
    if (*p=='"' || *p=='\'') {
        quote=(unsigned char)*p++;
        while (*p && (unsigned char)*p!=(unsigned char)quote) {
            if (name_len+1>=sizeof(filename)) return -1;
            filename[name_len++]=*p++;
        }
        if ((unsigned char)*p!=(unsigned char)quote) return -1;
        p++;
    } else {
        while (*p && !isspace((unsigned char)*p)) {
            if (name_len+1>=sizeof(filename)) return -1;
            filename[name_len++]=*p++;
        }
    }
    filename[name_len]='\0'; if (!name_len) return -1;
    while (*p && isspace((unsigned char)*p)) p++;
    end=p; while (*end && !isspace((unsigned char)*end)) end++;
    type_len=(size_t)(end-p); if (!type_len || type_len>=sizeof(type)) return -1;
    memcpy(type,p,type_len); type[type_len]='\0';
    for(i=0;i<type_len;i++) type[i]=(char)toupper((unsigned char)type[i]);
    return hash_track(hash,cue_path,filename,type)==0 ? 1 : -1;
}
int cb_disc_fingerprint(const char *cue_path, char *output, size_t output_size)
{
    FILE *cue;
    char line[CB_FP_LINE_MAX];
    uint64_t hash=CB_FNV_OFFSET;
    unsigned int files=0;
    if (!cue_path || !cue_path[0] || !output || output_size<17) return -1;
    output[0]='\0'; cue=fopen(cue_path,"rb"); if(!cue) return -1;
    while(fgets(line,sizeof(line),cue)) {
        size_t len=strlen(line), start=0, end, i;
        int parsed;
        if(len==sizeof(line)-1 && line[len-1]!='\n' && !feof(cue)){fclose(cue);return -1;}
        while(len && (line[len-1]=='\n'||line[len-1]=='\r')) line[--len]='\0';
        while(start<len && isspace((unsigned char)line[start])) start++;
        end=len; while(end>start && isspace((unsigned char)line[end-1])) end--;
        line[end]='\0'; if(start) memmove(line,line+start,end-start+1);
        if(!line[0]) continue;
        parsed=parse_file(&hash,cue_path,line);
        if(parsed<0){fclose(cue);return -1;}
        if(parsed>0){files++;continue;}
        for(i=0;line[i];i++) line[i]=(char)toupper((unsigned char)line[i]);
        hash=hash_text(hash,line);
    }
    if(ferror(cue)){fclose(cue);return -1;}
    if(fclose(cue)!=0 || files==0) return -1;
    snprintf(output,output_size,"%016llX",(unsigned long long)hash);
    return 0;
}
