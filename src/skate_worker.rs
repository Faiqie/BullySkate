//! Isolated x64 source simulation. The x86 Bully adapter owns native rendering.
use bully_skate_runtime::{SkateHost,SkaterPreferences};
use std::{ffi::c_void,io::Write,path::Path};
type Handle=*mut c_void;
#[link(name="kernel32")]
unsafe extern "system" {
 fn OpenFileMappingW(access:u32,inherit:i32,name:*const u16)->Handle;
 fn MapViewOfFile(mapping:Handle,access:u32,high:u32,low:u32,bytes:usize)->*mut c_void;
 fn OpenEventW(access:u32,inherit:i32,name:*const u16)->Handle;
 fn OpenProcess(access:u32,inherit:i32,pid:u32)->Handle;
 fn WaitForMultipleObjects(count:u32,handles:*const Handle,all:i32,timeout:u32)->u32;
 fn SetEvent(event:Handle)->i32;
 fn CloseHandle(handle:Handle)->i32;
 fn UnmapViewOfFile(pointer:*const c_void)->i32;
}
#[repr(C)]
#[derive(Clone,Copy)]
struct InputFrame {dt:f32,buttons:u32,axes:[f32;6]}
#[repr(C)]
struct Shared {
 ready:u32,command:u32,success:u32,
 mount:[f32;4],dt:f32,buttons:u32,axes:[f32;6],
 actor_count:u32,actors:[[f32;9];24],aspect:f32,
 output:[f32;14],pose:[[f32;13];36],board:[[f32;13];2],camera:[f32;7],root:[f32;3],
 has_camera:u32,error:[u8;2048],preferences:[f32;10],
 marker_flags:u32,marker_sets:u32,marker_returns:u32,marker_progress:f32,
 input_count:u32,inputs:[InputFrame;8],
 mount_area:u32,actor_revision:u32,vehicle_count:u32,vehicle_revision:u32,vehicles:[[f32;16];8],
 interaction:[u32;4],
 audio:[f32;64],
 world_phase:u32,difficulty:u32,camera_type:u32,
}
const _: [();6184]=[();std::mem::size_of::<Shared>()];
fn wide(value:&str)->Vec<u16>{value.encode_utf16().chain(Some(0)).collect()}
fn log(message:&str){
 if let Ok(mut file)=std::fs::OpenOptions::new().create(true).append(true).open(r"_derpy_script_loader\logs\skate-worker.log"){
  let _=writeln!(file,"{message}");
 }
}
fn source_root()->std::path::PathBuf{
 std::fs::read_to_string(r"_derpy_script_loader\scripts\BullyMotion\source-path.txt").ok()
  .map(|value|value.trim().into()).unwrap_or_else(||r"_derpy_script_loader\scripts\BullyMotion\skate-assets".into())
}
fn error(shared:&mut Shared,message:&str){
 shared.error.fill(0);let bytes=message.as_bytes();let len=bytes.len().min(shared.error.len()-1);
 shared.error[..len].copy_from_slice(&bytes[..len]);shared.ready=3;shared.success=0;log(message);
}
fn publish(host:&mut SkateHost,shared:&mut Shared)->Result<(),String>{
 shared.output=host.output();shared.pose=host.native_pose().ok_or("Jimmy retarget pose is missing")?;
 shared.board=host.native_board_pose();shared.root=host.rider_root();
 (shared.marker_flags,shared.marker_sets,shared.marker_returns,shared.marker_progress)=host.marker_status();
 shared.interaction=host.interaction_status();
 shared.audio=host.audio_frame();
 let aspect=if shared.aspect.is_finite()&&shared.aspect>0.{shared.aspect}else{16./9.};
 let camera=host.camera(aspect);shared.has_camera=camera.is_some() as u32;shared.camera=camera.unwrap_or([0.;7]);
 if shared.output.iter().chain(shared.pose.iter().flatten()).chain(shared.board.iter().flatten())
  .chain(shared.root.iter()).chain(shared.camera.iter()).any(|value|!value.is_finite()){
  return Err("Source simulation returned a non-finite pose".into());
 }
 Ok(())
}
fn run()->Result<(),String>{
 let args:Vec<String>=std::env::args().collect();
 if args.len()!=3||args[1]!="--parent"{return Err("Use the Bully skating launcher to start this worker".into())}
 let pid:u32=args[2].parse().map_err(|_|"Invalid parent process")?;
 let map_name=wide(&format!("Local\\BullySkate-{pid}-map"));
 let command_name=wide(&format!("Local\\BullySkate-{pid}-command"));
 let response_name=wide(&format!("Local\\BullySkate-{pid}-response"));
 unsafe{
  let mapping=OpenFileMappingW(0xf001f,0,map_name.as_ptr());
  if mapping.is_null(){return Err("Simulation shared memory was not created by Bully".into())}
  let memory=MapViewOfFile(mapping,0xf001f,0,0,std::mem::size_of::<Shared>()).cast::<Shared>();
  if memory.is_null(){CloseHandle(mapping);return Err("Cannot map simulation shared memory".into())}
  let command=OpenEventW(0x100002,0,command_name.as_ptr());
  let response=OpenEventW(0x100002,0,response_name.as_ptr());
  let parent=OpenProcess(0x100000,0,pid);
  let shared=&mut *memory;
  if command.is_null()||response.is_null()||parent.is_null(){
   error(shared,"Simulation synchronization or parent process could not be opened");return Err("Cannot open simulation events".into());
  }
  let started=std::time::Instant::now();log(&format!("x64 source worker starting; Bully PID {pid}; shared size {}; host size {}",std::mem::size_of::<Shared>(),std::mem::size_of::<SkateHost>()));
  let mut host=match std::panic::catch_unwind(||SkateHost::bully_visible(&source_root(),Path::new(r"_derpy_script_loader\scripts\BullyMotion\assets\world.bmgeo"),[298.36,5.8026,72.23],std::f32::consts::PI,shared.world_phase)){
   Ok(Ok(host))=>host,Ok(Err(message))=>{error(shared,&message);return Err(message)},Err(_)=>{error(shared,"Source startup panicked; see skate-worker.log");return Err("Source startup panic".into())}
  };
  host.configure(SkaterPreferences::from_wire(shared.preferences)?);
  host.configure_difficulty(shared.difficulty)?;
  host.configure_camera(shared.camera_type)?;
  shared.ready=2;log(&format!("Ready after {:.3}s",started.elapsed().as_secs_f64()));
  let(mut mounted,mut accumulator)=(false,0f32);let handles=[command,parent];
  let(mut actor_revision,mut vehicle_revision)=(u32::MAX,u32::MAX);
  let(mut frames,mut total,mut peak)=(0u64,0f64,0f64);
  let debug=std::env::var_os("BULLY_SKATE_DEBUG").is_some();
  loop{
   match WaitForMultipleObjects(2,handles.as_ptr(),0,u32::MAX){0=>{},1=>break,_=>return Err("Simulation event wait failed".into())}
   std::sync::atomic::fence(std::sync::atomic::Ordering::Acquire);
   let frame_start=std::time::Instant::now();
   let result=std::panic::catch_unwind(std::panic::AssertUnwindSafe(||->Result<(),String>{
    match shared.command{
     1|6=>{
      if shared.mount.iter().any(|v|!v.is_finite()){return Err("Invalid mounting position".into())}
      host.select_world(shared.mount_area,shared.world_phase)?;
      let[x,y,z,yaw]=shared.mount;host.relocate([x,z,-y],yaw+std::f32::consts::PI)?;
      host.actors(&[]);host.vehicles(&[]);host.suspend_interactions();
      actor_revision=u32::MAX;vehicle_revision=u32::MAX;
      host.tick(skate_core::input::xbox::XboxState{buttons:0,left:[0;2],right:[0;2],triggers:[0;2]})?;
      mounted=true;accumulator=0.;publish(&mut host,shared)
     },
     2=>{
      host.select_world(shared.mount_area,shared.world_phase)?;
      if !mounted||shared.input_count==0||shared.input_count>8||shared.actor_count>24||shared.vehicle_count>8{return Err("Invalid simulation input".into())}
      if actor_revision!=shared.actor_revision{host.actors_sized(&shared.actors[..shared.actor_count as usize]);actor_revision=shared.actor_revision;}
      if vehicle_revision!=shared.vehicle_revision{host.vehicles(&shared.vehicles[..shared.vehicle_count as usize]);vehicle_revision=shared.vehicle_revision;}
      let axis=|v:f32|(v.clamp(-1.,1.)*32767.) as i16;
      let trigger=|v:f32|(v.clamp(0.,1.)*255.) as u8;
      for input in &shared.inputs[..shared.input_count as usize]{
       if !input.dt.is_finite()||input.axes.iter().any(|v|!v.is_finite()){return Err("Invalid queued simulation input".into())}
       accumulator+=input.dt.clamp(0.,0.1);
       let[lx,ly,rx,ry,lt,rt]=input.axes;
       while accumulator+0.000001>=host.period(){
        let step=host.period();accumulator=(accumulator-step).max(0.);
        host.tick(skate_core::input::xbox::XboxState{buttons:input.buttons as u16,left:[axis(lx),axis(ly)],right:[axis(rx),axis(ry)],triggers:[trigger(lt),trigger(rt)]})?;
       }
      }
      publish(&mut host,shared)
     },
     3=>{mounted=false;host.actors(&[]);host.vehicles(&[]);host.suspend_interactions();host.suspend_marker();Ok(())},
     4=>{
      host.configure(SkaterPreferences::from_wire(shared.preferences)?);
      host.configure_difficulty(shared.difficulty)?;
      host.configure_camera(shared.camera_type)?;
      log(&format!("Skater preferences {:?}",shared.preferences));
      if mounted{publish(&mut host,shared)}else{Ok(())}
     },
     5=>{host.clear_marker();if mounted{publish(&mut host,shared)}else{Ok(())}},
     _=>Err("Unknown simulation command".into()),
    }
   })).unwrap_or_else(|_|Err("Source simulation panicked; see skate-worker.log".into()));
   match result{Ok(())=>shared.success=1,Err(message)=>{
    error(shared,&message);
    if shared.command==1&&message.starts_with("No skating collision for Bully area "){shared.ready=2;mounted=false;}
   }}
   let ms=frame_start.elapsed().as_secs_f64()*1000.;frames+=1;total+=ms;peak=peak.max(ms);
   if debug&&(frames%60==0||shared.axes[3]!=0.){log(&format!("command {frames} dt={} axes={:?} {} deck={:?}",shared.dt,shared.axes,host.status(),host.position()));}
   if frames%600==0{log(&format!("{frames} commands; average {:.3}ms peak {:.3}ms",total/frames as f64,peak));}
   std::sync::atomic::fence(std::sync::atomic::Ordering::Release);SetEvent(response);
   if shared.ready==3{break;}
  }
  log("Parent exited; worker shutting down");UnmapViewOfFile(memory.cast());
  for handle in [command,response,parent,mapping]{CloseHandle(handle);}
 }
 Ok(())
}
fn main(){
 std::panic::set_hook(Box::new(|info|log(&format!("{info}\n{}",std::backtrace::Backtrace::force_capture()))));
 // Source graph/animation construction temporarily keeps several large
 // fixed records on its stack. Keep that requirement outside Bully's threads.
 let result=std::thread::Builder::new().name("Skate simulation".into()).stack_size(32*1024*1024).spawn(run)
  .map_err(|e|e.to_string()).and_then(|worker|worker.join().map_err(|_|"Simulation thread panicked".to_string()))
  .and_then(|result|result);
 if let Err(message)=result{log(&message);std::process::exit(1);}
}
