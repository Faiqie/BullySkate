use bevy::prelude::*;
use skate_core::animation::output::NativeMatrix;
pub(crate) fn native_matrix(matrix: NativeMatrix) -> Mat4 {
    Mat4::from_cols(
        Vec3::from_array(matrix[0][..3].try_into().unwrap()).extend(0.0),
        Vec3::from_array(matrix[1][..3].try_into().unwrap()).extend(0.0),
        Vec3::from_array(matrix[2][..3].try_into().unwrap()).extend(0.0),
        Vec3::from_array(matrix[3][..3].try_into().unwrap()).extend(1.0),
    )
}

#[cfg(test)]
mod online_swap_tests {
    use super::*;
    use bevy::{ecs::system::SystemState,mesh::skinning::SkinnedMesh};
    #[test]
    fn online_appearance_swap_seeds_new_rig_and_keeps_animating() {
        let mut world=World::new();
        let mut roots=vec![];
        for _ in 0..2 {
            let root=world.spawn_empty().id();
            let hip=world.spawn((Name::new("HIPS"),Transform::default(),ChildOf(root))).id();
            let head=world.spawn((Name::new("HEAD"),Transform::default(),ChildOf(hip))).id();
            world.spawn((ChildOf(root),SkinnedMesh{inverse_bindposes:default(),joints:vec![hip,head]}));
            roots.push((root,hip,head));
        }
        let names=vec!["HIPS".to_owned(),"HEAD".to_owned()];
        let bind=|world:&mut World,root| {
            let mut query:SystemState<(Query<(Entity,&SkinnedMesh)>,Query<(&Name,&Transform)>,Query<&ChildOf>)>=SystemState::new(world);
            let (skins,nodes,parents)=query.get(world);
            AnimationStatus::for_scene(root,&names,&skins,&nodes,&parents).unwrap()
        };
        let old=bind(&mut world,roots[0].0);
        let pose=[Mat4::from_translation(Vec3::new(1.,2.,3.)),Mat4::from_rotation_z(0.7)];
        for (entity,t) in old.pose_transforms(&pose){*world.get_mut::<Transform>(entity).unwrap()=t;}
        let replacement=bind(&mut world,roots[1].0);
        assert_eq!(*world.get::<Transform>(roots[1].1).unwrap(),Transform::default());
        for (entity,t) in replacement.pose_transforms(&pose){*world.get_mut::<Transform>(entity).unwrap()=t;}
        assert_eq!(world.get::<Transform>(roots[0].1),world.get::<Transform>(roots[1].1));
        assert_eq!(world.get::<Transform>(roots[0].2),world.get::<Transform>(roots[1].2));
        let next=[Mat4::from_translation(Vec3::new(2.,3.,4.)),Mat4::from_rotation_z(1.2)];
        for (entity,t) in replacement.pose_transforms(&next){*world.get_mut::<Transform>(entity).unwrap()=t;}
        assert_ne!(world.get::<Transform>(roots[0].1),world.get::<Transform>(roots[1].1));
        assert_ne!(world.get::<Transform>(roots[0].2),world.get::<Transform>(roots[1].2));
    }
}
