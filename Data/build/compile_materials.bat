echo off

cd ../../
MaterialCompiler -s Data/materials/mg/sky.mg -o Data/materials/compiled/sky.mat
MaterialCompiler -s Data/materials/mg/wall.mg -o Data/materials/compiled/wall.mat
MaterialCompiler -s Data/materials/mg/shadow_caster.mg -o Data/materials/compiled/shadow_caster.mat

pause
