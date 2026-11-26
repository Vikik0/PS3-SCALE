"use strict";
const test=require("node:test");
const assert=require("node:assert/strict");
const {RETROARCH_LOBBY_URL,parseRetroArchLobby,endpointFromEntry,findRetroArchHost,fetchRetroArchLobby}=require("../api/_lib/netplay");

test("RetroArch lobby parser accepts wrapped and flat records",()=>{
 const entries=parseRetroArchLobby([{fields:{username:"CB-ABC123-HOST",ip:"1.2.3.4",port:55435}},{username:"CB-DEF456-HOST",ip:"5.6.7.8",port:55436}]);
 assert.equal(entries.length,2);assert.equal(entries[0].username,"CB-ABC123-HOST");assert.equal(entries[1].username,"CB-DEF456-HOST");
});
test("host lookup returns a direct endpoint when connectable",()=>{
 const result=findRetroArchHost([{username:"CB-ABC123-HOST",ip:"198.51.100.12",port:55435,connectable:true,game_name:"Crash Bash (USA).cue",core_name:"PCSX-ReARMed",core_version:"1.20.0"}],"CB-ABC123-HOST");
 assert.equal(result.ready,true);assert.equal(result.mode,"direct");assert.equal(result.address,"198.51.100.12");assert.equal(result.port,55435);assert.equal(result.coreName,"PCSX-ReARMed");
});
test("host lookup prefers a valid relay endpoint",()=>{
 const result=findRetroArchHost([{fields:{username:"CB-ABC123-HOST",ip:"10.0.0.2",port:55435,connectable:false,mitm_ip:"relay.libretro.com",mitm_port:"55435",mitm_session:"AbCdEf0123456789+/==",game_name:"Crash Bash",core_name:"PCSX-ReARMed"}}],"cb-abc123-host");
 assert.equal(result.ready,true);assert.equal(result.mode,"relay");assert.equal(result.address,"relay.libretro.com");assert.equal(result.mitmSession,"AbCdEf0123456789+/==");
});
test("password-protected hosts are reported explicitly",()=>{
 assert.deepEqual(endpointFromEntry({has_password:"1",ip:"1.2.3.4",port:1234}),{ready:false,state:"host_password_required"});
});
test("non-connectable host without relay is rejected",()=>{
 assert.deepEqual(endpointFromEntry({connectable:false,ip:"1.2.3.4",port:1234}),{ready:false,state:"host_not_connectable"});
});
test("unsafe host, port and relay session values are rejected",()=>{
 assert.equal(endpointFromEntry({ip:"example.com;evil",port:55435,connectable:true}).ready,false);
 assert.equal(endpointFromEntry({ip:"1.2.3.4",port:70000,connectable:true}).ready,false);
 assert.equal(endpointFromEntry({mitm_ip:"relay.libretro.com",mitm_port:55435,mitm_session:"bad session",connectable:false}).ready,false);
});
test("unknown nickname stays unmatched",()=>assert.equal(findRetroArchHost([],"CB-ABC123-HOST"),null));
test("lobby fetch uses HTTPS JSON and returns parsed entries",async()=>{
 let requested="";
 const rooms=await fetchRetroArchLobby({timeoutMs:1000,fetchImpl:async(url,options)=>{
  requested=url;assert.equal(options.headers.Accept,"application/json");
  return {ok:true,headers:{get:()=>"2"},body:null,text:async()=>'[{"username":"CB-ABC123-HOST","ip":"1.2.3.4","port":55435}]'};
 }});
 assert.equal(requested,RETROARCH_LOBBY_URL);assert.equal(rooms.length,1);
});
test("lobby fetch rejects oversized declared responses",async()=>{
 await assert.rejects(()=>fetchRetroArchLobby({timeoutMs:1000,fetchImpl:async()=>({ok:true,headers:{get:()=>String(5*1024*1024)},body:null,text:async()=>"[]"})}),/size limit/);
});
