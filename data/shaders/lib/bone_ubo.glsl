layout(std140, binding=2) uniform BoneMatrices 
{
    mat4 bone_matrices[MAX_BONES];
};