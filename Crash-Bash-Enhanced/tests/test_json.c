#include "json_min.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    const char *health="{\"ok\":true,\"storageConfigured\":true,\"tokenConfigured\":true}";
    const char *rooms="{\"rooms\":[{\"code\":\"AB2345\",\"host\":\"Guest \\\"One\\\"\",\"players\":1,\"maxPlayers\":2},{\"code\":\"CD6789\",\"host\":\"Two\",\"players\":1,\"maxPlayers\":2}]}";
    const char *room="{\"room\":{\"code\":\"AB2345\",\"status\":\"ready\",\"isPrivate\":false,\"players\":[{\"id\":\"p1\",\"name\":\"Guest One\",\"ready\":true},{\"id\":\"p2\",\"name\":\"Guest Two\",\"ready\":true}]}}";
    cb_json_token tokens[128]; char text[64]; int count,root,array,item,value;
    count=cb_json_parse(health,tokens,128); assert(count>0); root=0; value=cb_json_object_get(health,tokens,count,root,"ok"); assert(value>=0&&cb_json_bool(health,&tokens[value],0)==1);
    count=cb_json_parse(rooms,tokens,128); assert(count>0); array=cb_json_object_get(rooms,tokens,count,0,"rooms"); assert(array>=0&&cb_json_array_size(tokens,count,array)==2);
    item=cb_json_array_get(tokens,count,array,0); value=cb_json_object_get(rooms,tokens,count,item,"host"); assert(value>=0&&cb_json_string(rooms,&tokens[value],text,sizeof(text))>0); assert(strcmp(text,"Guest \"One\"")==0);
    value=cb_json_object_get(rooms,tokens,count,item,"players"); assert(value>=0&&cb_json_int(rooms,&tokens[value],0)==1);
    count=cb_json_parse(room,tokens,128); assert(count>0); root=cb_json_object_get(room,tokens,count,0,"room"); array=cb_json_object_get(room,tokens,count,root,"players");
    assert(array>=0&&cb_json_array_size(tokens,count,array)==2); item=cb_json_array_get(tokens,count,array,1); value=cb_json_object_get(room,tokens,count,item,"ready"); assert(value>=0&&cb_json_bool(room,&tokens[value],0)==1);
    puts("JSON parser tests: PASS"); return 0;
}
