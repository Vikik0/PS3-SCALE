"use strict";
const test=require("node:test");
const assert=require("node:assert/strict");
const fs=require("node:fs");
const path=require("node:path");
const root=path.resolve(__dirname,"..");
const html=fs.readFileSync(path.join(root,"index.html"),"utf8");
const api=fs.readFileSync(path.join(root,"api","rooms.js"),"utf8");
test("room identity and readiness restore after refresh",()=>{
 assert.match(html,/sessionStorage\.setItem\("cb_player",state\.player\.id\)/);
 assert.match(html,/sessionStorage\.getItem\("cb_player"\)/);
 assert.match(html,/p=>p\.id===state\.player\?\.id/);
});
test("room arrows focus room controls only while room view is active",()=>{
 assert.match(html,/if\(!\$\("roomView"\)\.classList\.contains\("active"\)\)return/);
 assert.match(html,/querySelectorAll\("button\[data-room-action\]:not\(:disabled\)"\)/);
 assert.match(html,/buttons\[next\]\.focus\(\)/);
});
test("two ready players are enough to ready a room",()=>{
 assert.match(html,/players\.length>=2&&players\.every\(p=>p\.ready\)/);
 assert.match(api,/players\.length >= 2 && players\.every\(\(player\) => player\.ready === true\)/);
});
test("room list errors are rendered as text",()=>{
 assert.match(html,/errorNode\.textContent=e\.message/);
 assert.doesNotMatch(html,/innerHTML=.*e\.message/);
});
test("host Netplay nickname is derived from the server-issued launch identity",()=>{
 const native=fs.readFileSync(path.join(root,"..","source","online_menu.c"),"utf8");
 assert.match(api,/String\(room\.netplayId\)\.slice\(0, 16\)/);
 assert.match(native,/CB-%\.16s-HOST/);
 const start=native.indexOf("cb_online_mark_game_started(&session, &view");
 const nickname=native.indexOf("make_netplay_nickname",start);
 const launch=native.indexOf("cb_launch_game(discs[launch_disc].path, 1",nickname);
 assert.ok(start>=0&&nickname>start&&launch>nickname,"host must use the server-issued identity before launching RetroArch");
});
test("native PS3 protocol actions are implemented",()=>{
 for(const action of ["set_ready","set_disc","clear_disc","game_start","game_start_cancel","netplay"])assert.ok(api.includes('action === "'+action+'"'),"missing "+action);
 assert.match(api,/lookupRetroArchHost\(nickname/);
});
test("public room listing follows Blob cursors",()=>{
 assert.match(api,/do \{/);assert.match(api,/if \(cursor\) options\.cursor = cursor/);assert.match(api,/cursor = page\.cursor \|\| null/);
});
