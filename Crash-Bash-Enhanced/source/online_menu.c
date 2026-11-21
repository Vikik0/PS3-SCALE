#include "online_menu.h"
#include "online.h"
#include <stdio.h>
#include <string.h>
#include <tiny3d.h>
#include <libfont.h>
#include <io/pad.h>
#include <sysutil/sysutil.h>
#include <net/net.h>
#define MENU_ROOM_LIMIT 8
#define SCREEN_ROOMS 0
#define SCREEN_ROOM 1
static void draw_rect(float x,float y,float w,float h,u32 color) {
    tiny3d_SetPolygon(TINY3D_QUADS); tiny3d_VertexPos(x,y,65535.0f); tiny3d_VertexColor(color);
    tiny3d_VertexPos(x+w,y,65535.0f); tiny3d_VertexPos(x+w,y+h,65535.0f); tiny3d_VertexPos(x,y+h,65535.0f); tiny3d_End();
}
static void draw_text(float x,float y,int size,u32 color,const char *value) {
    char safe[256]; SetCurrentFont(0); SetFontColor(color,0x00000000); SetFontSize(size,size+8);
    snprintf(safe,sizeof(safe),"%s",value?value:""); DrawString(x,y,safe);
}
static int ensure_lobby(int *network_state,int *online_state,char *status,size_t size) {
    int rc; char error[160]="";
    if(!network_state||!online_state) return -1;
    if(*network_state!=1) { rc=netInitialize(); if(rc!=0) { *network_state=-1; *online_state=-1; snprintf(status,size,"Network initialization failed: 0x%08X",(unsigned int)rc); return -1; } *network_state=1; }
    if(cb_online_init()!=0) { *online_state=-1; snprintf(status,size,"Could not initialize the HTTPS client."); return -1; }
    if(cb_online_health(error,sizeof(error))!=0) { *online_state=-1; snprintf(status,size,"Lobby service: %.120s",error); return -1; }
    *online_state=1; snprintf(status,size,"Cloud lobby connected. PSN login is not used."); return 0;
}
static int load_rooms(CBOnlineRoom *rooms,int *count,int *selected,char *status,size_t size) {
    size_t n=0; char error[160]="";
    if(cb_online_list_rooms(rooms,MENU_ROOM_LIMIT,&n,error,sizeof(error))!=0) { *count=0; snprintf(status,size,"Room list: %.120s",error); return -1; }
    *count=(int)n; if(*count==0) *selected=0; else if(*selected>=*count) *selected=*count-1;
    snprintf(status,size,"Found %d public room(s). Triangle creates a room.",*count); return 0;
}
static void draw_rooms(const CBOnlineRoom *rooms,int count,int selected,const char *status) {
    int i; tiny3d_Clear(0x071019ff,TINY3D_CLEAR_ALL); tiny3d_Project2D();
    draw_rect(24,22,800,468,0x101e2aff); draw_rect(24,22,800,5,0x46d6b2ff);
    draw_text(56,44,27,0xf5fbffff,"ONLINE ROOMS");
    draw_text(56,82,14,0x9bb4c2ff,"PUBLIC MATCHMAKING  |  TWO-PLAYER LOBBY  |  NO PSN");
    if(count==0) { draw_rect(58,142,720,120,0x0b1720ff); draw_text(82,176,18,0xc2d0d8ff,"No public rooms are waiting."); draw_text(82,214,15,0x61d9baff,"Press TRIANGLE to create a room."); }
    else for(i=0;i<count;i++) { char row[96]; u32 bg=i==selected?0x28594fff:0x172a36ff; u32 fg=i==selected?0xffffffff:0xc2d0d8ff;
        draw_rect(58,130+i*38,720,31,bg); snprintf(row,sizeof(row),"%.6s   %-20.20s   %d/%d PLAYERS",rooms[i].code,rooms[i].host,rooms[i].players,rooms[i].max_players); draw_text(76,136+i*38,15,fg,row); }
    draw_rect(58,438,720,32,0x0b1720ff); draw_text(72,444,13,0x9bb4c2ff,status);
    draw_text(56,474,12,0x6d8794ff,"UP/DOWN: SELECT   CROSS: JOIN   TRIANGLE: CREATE   SQUARE: REFRESH   CIRCLE: BACK"); tiny3d_Flip();
}
static void draw_room(const CBOnlineRoomView *view,const CBOnlineSession *session,const char *status) {
    int i; tiny3d_Clear(0x071019ff,TINY3D_CLEAR_ALL); tiny3d_Project2D();
    draw_rect(24,22,800,468,0x101e2aff); draw_rect(24,22,800,5,0x46d6b2ff);
    draw_text(56,44,26,0xf5fbffff,"ROOM LOBBY"); draw_text(56,86,16,0x61d9baff,"INVITE CODE"); draw_text(220,78,30,0xf5fbffff,session->code);
    for(i=0;i<CB_ONLINE_PLAYER_MAX;i++) { char slot[80]; int y=154+i*92; draw_rect(58,y,720,72,0x172a36ff);
        if(i<view->player_count) { snprintf(slot,sizeof(slot),"PLAYER %d   %s",i+1,view->players[i].name); draw_text(82,y+12,18,0xf0f6f8ff,slot); draw_text(82,y+42,14,view->players[i].ready?0x7ce0a8ff:0xffc18aff,view->players[i].ready?"READY":"NOT READY"); }
        else { snprintf(slot,sizeof(slot),"PLAYER %d   WAITING FOR A FRIEND",i+1); draw_text(82,y+22,17,0x9bb4c2ff,slot); } }
    if(view->is_ready) draw_text(58,360,17,0x7ce0a8ff,"BOTH PLAYERS READY - GAMEPLAY TRANSPORT IS NEXT");
    else draw_text(58,360,16,0x9bb4c2ff,"Waiting for both players to mark READY.");
    draw_rect(58,408,720,38,0x0b1720ff); draw_text(72,416,14,0x9bb4c2ff,status);
    draw_text(56,474,12,0x6d8794ff,"CROSS: TOGGLE READY   SQUARE: REFRESH   CIRCLE: LEAVE ROOM"); tiny3d_Flip();
}
int cb_online_menu(const char *guest_name,int *network_state,int *online_state,volatile int *exit_requested,char *status,size_t status_size) {
    CBOnlineRoom rooms[MENU_ROOM_LIMIT]; CBOnlineSession session; CBOnlineRoomView view;
    char error[160]=""; int screen=SCREEN_ROOMS,room_count=0,selected=0,ready=0,curl_initialized=0; unsigned int poll_frames=0;
    u16 old_up=0,old_down=0,old_cross=0,old_circle=0,old_triangle=0,old_square=0;
    memset(&session,0,sizeof(session)); memset(&view,0,sizeof(view)); memset(rooms,0,sizeof(rooms));
    if(ensure_lobby(network_state,online_state,status,status_size)!=0) {
        if(cb_online_init()==0) curl_initialized=1;
        while(!exit_requested||!*exit_requested) {
            u16 circle=0,square=0; padInfo info; padData data; sysUtilCheckCallback();
            if(ioPadGetInfo(&info)==0) { int p; for(p=0;p<MAX_PADS;p++) if(info.status[p]&&ioPadGetData(p,&data)==0&&data.len>=8) { circle=data.BTN_CIRCLE; square=data.BTN_SQUARE; break; } }
            tiny3d_Clear(0x071019ff,TINY3D_CLEAR_ALL); tiny3d_Project2D(); draw_rect(24,22,800,468,0x101e2aff); draw_rect(24,22,800,5,0xff9a55ff);
            draw_text(56,54,27,0xf5fbffff,"ONLINE SERVICE UNAVAILABLE"); draw_text(56,118,16,0xffc18aff,status);
            draw_text(56,180,15,0x9bb4c2ff,"Check the network connection and try again."); draw_text(56,474,12,0x6d8794ff,"SQUARE: RETRY   CIRCLE: BACK"); tiny3d_Flip();
            if(circle&&!old_circle) break;
            if(square&&!old_square&&ensure_lobby(network_state,online_state,status,status_size)==0) break;
            old_circle=circle; old_square=square;
        }
        if(curl_initialized) cb_online_shutdown(); return 0;
    }
    curl_initialized=1; load_rooms(rooms,&room_count,&selected,status,status_size);
    while(!exit_requested||!*exit_requested) {
        padInfo info; padData data; u16 up=0,down=0,cross=0,circle=0,triangle=0,square=0; sysUtilCheckCallback();
        if(ioPadGetInfo(&info)==0) { int p; for(p=0;p<MAX_PADS;p++) { if(!info.status[p]) continue; if(ioPadGetData(p,&data)==0&&data.len>=8) { up=data.BTN_UP; down=data.BTN_DOWN; cross=data.BTN_CROSS; circle=data.BTN_CIRCLE; triangle=data.BTN_TRIANGLE; square=data.BTN_SQUARE; break; } } }
        if(screen==SCREEN_ROOMS) {
            if(up&&!old_up&&room_count>0) selected=(selected+room_count-1)%room_count;
            if(down&&!old_down&&room_count>0) selected=(selected+1)%room_count;
            if(cross&&!old_cross&&room_count>0) {
                if(cb_online_join_room(rooms[selected].code,guest_name,&session,&view,error,sizeof(error))==0) { ready=0; screen=SCREEN_ROOM; poll_frames=0; snprintf(status,status_size,"Joined room %.6s. Waiting for both players.",session.code); }
                else snprintf(status,status_size,"Join room: %.120s",error);
            }
            if(triangle&&!old_triangle) {
                if(cb_online_create_room(guest_name,0,&session,&view,error,sizeof(error))==0) { ready=0; screen=SCREEN_ROOM; poll_frames=0; snprintf(status,status_size,"Room %.6s created. Share the code with a friend.",session.code); }
                else snprintf(status,status_size,"Create room: %.120s",error);
            }
            if(square&&!old_square) load_rooms(rooms,&room_count,&selected,status,status_size);
            draw_rooms(rooms,room_count,selected,status); if(circle&&!old_circle) break;
        } else {
            if(cross&&!old_cross) { int next_ready=!ready; if(cb_online_set_ready(&session,next_ready,&view,error,sizeof(error))==0) { ready=next_ready; snprintf(status,status_size,"%s",ready?"You are ready.":"Ready status cancelled."); } else snprintf(status,status_size,"Ready update: %.120s",error); }
            if(square&&!old_square) { if(cb_online_get_room(&session,&view,error,sizeof(error))!=0) snprintf(status,status_size,"Room refresh: %.120s",error); }
            if(circle&&!old_circle) { cb_online_leave_room(&session,error,sizeof(error)); memset(&session,0,sizeof(session)); memset(&view,0,sizeof(view)); ready=0; screen=SCREEN_ROOMS; load_rooms(rooms,&room_count,&selected,status,status_size); }
            if(screen==SCREEN_ROOM&&++poll_frames>=300) { poll_frames=0; if(cb_online_set_ready(&session,ready,&view,error,sizeof(error))!=0) snprintf(status,status_size,"Heartbeat: %.120s",error); else if(cb_online_get_room(&session,&view,error,sizeof(error))!=0) snprintf(status,status_size,"Room refresh: %.120s",error); }
            if(screen==SCREEN_ROOM) draw_room(&view,&session,status); else draw_rooms(rooms,room_count,selected,status);
        }
        old_up=up; old_down=down; old_cross=cross; old_circle=circle; old_triangle=triangle; old_square=square;
    }
    if(session.token[0]) cb_online_leave_room(&session,error,sizeof(error));
    if(curl_initialized) cb_online_shutdown(); return 0;
}
