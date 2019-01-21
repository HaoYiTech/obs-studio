更新步骤如下：
====================================================
1. 修改 E:\obs-studio\libobs\obs-config.h 里面的版本号码；
2. 重新编译 teacher|student 都需要完整编译发行版本；
3. obs.dll编译时已经自动投递到teacher|student发行目录；
4. 编写更新日志：
   E:\obs-studio\vsbuild\update_studio\teacher\changelog.txt => 讲师端更新日志
   E:\obs-studio\vsbuild\update_studio\student\changelog.txt => 学生端更新日志
5. 双击 json_student.bat 和 json_teacher.bat 分别生成如下脚本：
   json_teacher.bat => E:\obs-studio\vsbuild\update_studio\teacher\manifest.json
   json_student.bat => E:\obs-studio\vsbuild\update_studio\student\manifest.json
6. 将生成的 manifest.json 更新到升级服务器对应的目录下面；
7. 将编译好的学生端和讲师端二进制文件上传到升级服务器对应的目录下面；
8. 通过github把新的变更上传到代码管理服务器当中；
9. 转移到 E:\GitHub\HaoYiYun\Install 目录进行打包；
