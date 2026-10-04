use bully_skate_runtime::SkateHost;
use skate_core::input::xbox::XboxState;
fn run()->Result<(),String>{
 let root=std::env::args().nth(1).ok_or("Core assets path required")?;
 let mut host=SkateHost::flat(std::path::Path::new(&root))?;
 let input=||XboxState{buttons:0,left:[0;2],right:[0;2],triggers:[0;2]};
 for _ in 0..120{host.tick(input())?;}
 let high=host.camera(16./9.).ok_or("No high camera frame")?;
 let high_shot=host.camera_shot().to_string();
 let before=host.output();host.configure_camera(0)?;
 if host.output()!=before||host.active_camera()!=0{return Err("Changing camera reset physics".into())}
 for _ in 0..120{host.tick(input())?;}
 let low=host.camera(16./9.).ok_or("No low camera frame")?;
 let low_shot=host.camera_shot().to_string();
 println!("High shot {high_shot}: {high:?}; Low shot {low_shot}: {low:?}");
 if high_shot==low_shot||low[2]>=high[2]-0.15||low.iter().any(|v|!v.is_finite()){return Err("Low camera did not select a lower stock shot".into())}
 host.configure_camera(1)?;for _ in 0..120{host.tick(input())?;}
 if host.camera_shot()!=high_shot{return Err("High camera did not return to its original shot".into())}
 if host.configure_camera(2).is_ok()||host.active_camera()!=1{return Err("Invalid camera modified the selection".into())}
 println!("PASS: live Low/High switch uses distinct stock shots, Low has a lower eye, invalid selection is rejected, and physics is preserved");Ok(())
}
fn main()->Result<(),String>{std::thread::Builder::new().stack_size(32*1024*1024).spawn(run).map_err(|e|e.to_string())?.join().map_err(|_|"Probe panicked".to_string())?}
