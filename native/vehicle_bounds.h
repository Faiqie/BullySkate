/* Read bounds prepared from this user's COL archive. No game geometry is embedded. */
typedef struct VehicleBounds {int model;float min[3],max[3];} VehicleBounds;
static VehicleBounds vehicle_bounds[9];static unsigned vehicle_bounds_count;static int vehicle_bounds_loaded;
static const VehicleBounds *vehicle_model_bounds(int model){
 unsigned i,j;FILE *file=NULL;char magic[16];VehicleBounds value;
 if(!vehicle_bounds_loaded){
  vehicle_bounds_loaded=1;
  if(fopen_s(&file,"_derpy_script_loader/scripts/BullyMotion/assets/vehicle-bounds.txt","r")==0){
   if(fgets(magic,sizeof(magic),file)&&!strcmp(magic,"BMVB1\n")){
    while(vehicle_bounds_count<9&&fscanf_s(file,"%d %f %f %f %f %f %f",&value.model,&value.min[0],&value.min[1],&value.min[2],&value.max[0],&value.max[1],&value.max[2])==7){
     int valid=value.model==286||(value.model>=290&&value.model<=297);
     for(j=0;j<3;j++)if(!isfinite(value.min[j])||!isfinite(value.max[j])||value.min[j]>=value.max[j]||fabsf(value.min[j])>20||fabsf(value.max[j])>20)valid=0;
     for(j=0;j<vehicle_bounds_count;j++)if(vehicle_bounds[j].model==value.model)valid=0;
     if(!valid){vehicle_bounds_count=0;break;}
     vehicle_bounds[vehicle_bounds_count++]=value;
    }
   }
   fclose(file);
  }
  if(vehicle_bounds_count!=9)vehicle_bounds_count=0;
 }
 for(i=0;i<vehicle_bounds_count;i++)if(vehicle_bounds[i].model==model)return vehicle_bounds+i;
 return NULL;
}
