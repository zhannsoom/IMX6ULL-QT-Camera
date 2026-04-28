# UI 相机工程 GitHub 上传 V1 详细教程

版本：V1.0  
适用工程：`v4l2_camera`  
适用场景：将当前 UI 相机项目作为 GitHub 作品集项目，用于后续实习投递、简历展示和项目复盘。

---

## 1. 为什么要把这个项目上传到 GitHub

你现在这个 UI 相机项目已经不只是一个简单 demo，它包含了嵌入式 Linux 应用开发中比较完整的一条链路：

```text
V4L2 摄像头采集
        ↓
framebuffer LCD 显示
        ↓
Linux input 触摸事件
        ↓
UI 按钮绘制
        ↓
拍照、录像、相册
```

对于找嵌入式 Linux、Linux 驱动应用、C/C++、物联网、智能硬件方向实习来说，这类项目比普通算法题或单文件小 demo 更有展示价值。

GitHub 上传的目的不是简单“备份代码”，而是让面试官看到：

1. 你能独立完成一个嵌入式 Linux 应用。
2. 你理解 V4L2、framebuffer、input event 等 Linux 设备接口。
3. 你有模块化工程组织能力。
4. 你有文档意识，能把项目讲清楚。
5. 你会使用 Git/GitHub 做版本管理。

所以本教程会按“作品集项目”的标准来做，而不是只告诉你 `git push`。

---

## 2. 上传前先理解 GitHub 上应该放什么

一个适合找实习的 GitHub 项目，应该上传：

| 类型 | 是否上传 | 原因 |
| --- | --- | --- |
| `.c/.h` 源码 | 上传 | 面试官主要看源码结构和实现能力 |
| `Makefile` | 上传 | 说明项目可以被编译 |
| `README.md` | 上传 | GitHub 首页展示项目说明 |
| `docs/` 文档 | 上传 | 展示学习总结和工程说明能力 |
| `.gitignore` | 上传 | 防止上传垃圾文件 |
| `obj/` 目录 | 不上传 | 编译中间产物，可重新生成 |
| `*.o` 文件 | 不上传 | 编译中间产物 |
| `v4l2_test` 可执行文件 | 通常不上传 | 平台相关，可由源码重新编译 |
| 照片和录像输出 | 不上传 | 运行产物，文件大且没有源码价值 |

当前工程已经添加了 `.gitignore`，会自动忽略：

```text
obj/
*.o
v4l2_test
v4l2_test_arm
v4l2_camera
photo/*.bmp
video/*.r565
```

为什么不建议上传可执行文件？

因为可执行文件和你的开发板架构、编译器版本有关。GitHub 项目应该以“源码可复现”为主。面试官更关心你的源码和文档，而不是一个无法在他电脑上运行的 ARM 可执行文件。

---

## 3. 推荐的 GitHub 仓库信息

### 3.1 仓库名称

建议仓库名：

```text
v4l2-ui-camera
```

也可以用：

```text
embedded-linux-ui-camera
imx6u-v4l2-camera
linux-framebuffer-camera
```

如果你投递嵌入式 Linux 方向，推荐：

```text
v4l2-ui-camera
```

原因：名字简洁，并且包含项目最核心关键词 `v4l2` 和 `camera`。

### 3.2 仓库描述

GitHub 创建仓库时有一个 Description，建议填写：

```text
An embedded Linux UI camera based on V4L2, framebuffer and input event, supporting preview, photo capture, recording and album browsing.
```

如果想用中文：

```text
基于 V4L2、framebuffer 和 input event 的嵌入式 Linux UI 相机项目，支持预览、拍照、录像和相册浏览。
```

### 3.3 仓库可见性

选择：

```text
Public
```

用于找实习的作品集项目必须设为公开，否则面试官打不开。

### 3.4 是否创建 README

如果你在 GitHub 网页创建仓库，页面会问：

```text
Add a README file
```

建议不要勾选。

原因：本地工程已经有 `README.md`。如果 GitHub 上也创建一个 README，第一次 push 时容易出现远程仓库和本地仓库历史不一致，需要额外 pull/merge。

### 3.5 是否创建 `.gitignore`

也不要勾选。

原因：本地工程已经有 `.gitignore`。

---

## 4. 第一次上传的完整流程

假设你的工程目录是：

```sh
E:/github/v4l2_camera
```

如果你现在是在 Linux 环境下，进入你自己的工程目录即可：

```sh
cd /path/to/v4l2_camera
```

后面的命令都在工程根目录执行。

---

## 5. 第一步：检查当前目录

执行：

```sh
pwd
ls
```

你应该能看到：

```text
main.c
main.h
Makefile
README.md
.gitignore
lcd/
v4l2/
input/
ui/
photo/
video/
album/
docs/
```

如果看不到 `main.c` 和 `Makefile`，说明你不在工程根目录。

---

## 6. 第二步：初始化 Git 仓库

当前目录还不是 Git 仓库，所以先执行：

```sh
git init
```

执行成功后，当前目录会多一个隐藏目录：

```text
.git/
```

这个目录是 Git 的版本数据库，不要手动删除。

检查状态：

```sh
git status
```

你会看到很多 `Untracked files`。这说明 Git 已经开始监控这个目录，但这些文件还没有被提交。

---

## 7. 第三步：设置 Git 用户信息

如果你的电脑第一次使用 Git，需要设置用户名和邮箱。

```sh
git config --global user.name "你的英文名或GitHub用户名"
git config --global user.email "你的GitHub邮箱"
```

例如：

```sh
git config --global user.name "zhangsan"
git config --global user.email "zhangsan@example.com"
```

查看配置：

```sh
git config --global --list
```

注意：这个邮箱最好和 GitHub 账号邮箱一致。这样 GitHub 才能正确把 commit 记录关联到你的账号。

---

## 8. 第四步：检查哪些文件会被提交

执行：

```sh
git status --short
```

你应该能看到源码和文档，但不应该看到：

```text
obj/
*.o
v4l2_test
v4l2_test_arm
v4l2_camera
```

如果它们没有出现，说明 `.gitignore` 生效了。

也可以执行：

```sh
git check-ignore -v obj/main.o
git check-ignore -v v4l2_test
```

如果输出 `.gitignore` 中的规则，说明这些文件会被忽略。

为什么这一步重要？

因为第一次提交最容易把编译产物、临时文件、大文件一起传到 GitHub。后面再清理会比较麻烦。

---

## 9. 第五步：添加文件到暂存区

执行：

```sh
git add .
```

这表示把当前目录下所有没有被 `.gitignore` 忽略的文件加入暂存区。

检查：

```sh
git status --short
```

你应该看到很多文件前面是 `A`，表示 Added。

例如：

```text
A  .gitignore
A  README.md
A  Makefile
A  main.c
A  main.h
A  lcd/lcd.c
A  lcd/lcd.h
A  v4l2/v4l2_camera.c
A  docs/ui_camera_learning_tutorial.md
A  docs/ui_camera_learning_tutorial.pdf
```

---

## 10. 第六步：创建 V1 首次提交

执行：

```sh
git commit -m "Release V1: embedded Linux V4L2 UI camera"
```

这个 commit message 的意思是：

```text
发布 V1：嵌入式 Linux V4L2 UI 相机
```

为什么 commit message 要认真写？

因为 GitHub 上每一次提交都会显示。对于作品集项目，清晰的提交记录会让人觉得你做事有条理。

查看提交记录：

```sh
git log --oneline
```

你会看到类似：

```text
abc1234 Release V1: embedded Linux V4L2 UI camera
```

---

## 11. 第七步：把默认分支改为 main

现在 GitHub 默认分支一般叫 `main`。如果你的本地分支不是 `main`，执行：

```sh
git branch -M main
```

查看当前分支：

```sh
git branch
```

应该看到：

```text
* main
```

---

## 12. 第八步：在 GitHub 网页创建远程仓库

打开 GitHub：

```text
https://github.com
```

登录账号后：

1. 点击右上角 `+`
2. 选择 `New repository`
3. Repository name 填：

```text
v4l2-ui-camera
```

4. Description 填：

```text
An embedded Linux UI camera based on V4L2, framebuffer and input event, supporting preview, photo capture, recording and album browsing.
```

5. 选择 `Public`
6. 不勾选 `Add a README file`
7. 不勾选 `.gitignore`
8. 点击 `Create repository`

创建完成后，GitHub 会给你一个远程地址。

HTTPS 地址一般类似：

```text
https://github.com/你的用户名/v4l2-ui-camera.git
```

SSH 地址一般类似：

```text
git@github.com:你的用户名/v4l2-ui-camera.git
```

---

## 13. 第九步：绑定远程仓库

如果你使用 HTTPS：

```sh
git remote add origin https://github.com/你的用户名/v4l2-ui-camera.git
```

如果你使用 SSH：

```sh
git remote add origin git@github.com:你的用户名/v4l2-ui-camera.git
```

查看远程仓库：

```sh
git remote -v
```

应该看到：

```text
origin  https://github.com/你的用户名/v4l2-ui-camera.git (fetch)
origin  https://github.com/你的用户名/v4l2-ui-camera.git (push)
```

或者：

```text
origin  git@github.com:你的用户名/v4l2-ui-camera.git (fetch)
origin  git@github.com:你的用户名/v4l2-ui-camera.git (push)
```

---

## 14. 第十步：推送到 GitHub

执行：

```sh
git push -u origin main
```

第一次 push 加 `-u` 的作用是把本地 `main` 分支和远程 `origin/main` 关联起来。以后再推送时，只需要：

```sh
git push
```

如果使用 HTTPS，GitHub 可能要求输入用户名和 token。现在 GitHub 不支持直接使用账号密码 push，需要使用 Personal Access Token。

如果你不想处理 token，建议使用 SSH。

---

## 15. SSH 方式配置 GitHub

如果你使用 SSH 地址，需要先检查本机有没有 SSH key：

```sh
ls ~/.ssh
```

如果没有 `id_ed25519.pub`，生成一个：

```sh
ssh-keygen -t ed25519 -C "你的GitHub邮箱"
```

一路回车即可。

查看公钥：

```sh
cat ~/.ssh/id_ed25519.pub
```

复制整行内容，然后进入 GitHub：

```text
Settings → SSH and GPG keys → New SSH key
```

Title 可以填：

```text
Linux Laptop
```

Key 粘贴刚才的公钥。

测试：

```sh
ssh -T git@github.com
```

第一次会问：

```text
Are you sure you want to continue connecting?
```

输入：

```sh
yes
```

如果看到类似：

```text
Hi 用户名! You've successfully authenticated
```

说明 SSH 配置成功。

---

## 16. 第十一步：给当前版本打 V1 标签

上传代码后，建议给当前版本打一个 Git tag。

执行：

```sh
git tag -a v1.0.0 -m "V1: UI camera with preview, photo, recording and album"
```

推送 tag：

```sh
git push origin v1.0.0
```

为什么要打 tag？

因为 tag 表示一个稳定版本。以后你继续开发 V2，如果改坏了，也可以随时回到 V1。

查看 tag：

```sh
git tag
```

查看某个 tag 对应的内容：

```sh
git show v1.0.0
```

---

## 17. 第十二步：在 GitHub 创建 Release

打开你的仓库页面：

```text
https://github.com/你的用户名/v4l2-ui-camera
```

点击右侧或顶部的：

```text
Releases
```

然后点击：

```text
Create a new release
```

选择 tag：

```text
v1.0.0
```

Release title 填：

```text
V1.0.0 - Embedded Linux V4L2 UI Camera
```

Release notes 可以填写：

```md
## V1.0.0

This is the first stable version of the embedded Linux UI camera project.

### Features

- Real-time camera preview based on V4L2 and framebuffer
- Touch UI based on Linux input event
- Photo capture saved as BMP
- Raw RGB565 video recording saved as `.r565`
- Album browsing for captured BMP photos
- Detailed project tutorial in `docs/`

### Target Platform

- Linux framebuffer: `/dev/fb0`
- V4L2 camera: `/dev/videoX`
- Linux input touch screen: `/dev/input/eventX`
```

然后点击：

```text
Publish release
```

Release 的价值是：面试官可以直接看到你标记过的稳定版本，而不是只看到一堆提交。

---

## 18. 第十三步：检查 GitHub 页面效果

上传成功后，打开仓库首页，重点检查：

1. README 是否正常显示。
2. 代码目录是否清晰。
3. `docs/` 文档是否能打开。
4. 是否没有上传 `obj/`、`.o`、可执行文件。
5. 右侧 About 是否填写了项目描述。
6. 是否有 `v1.0.0` tag 和 Release。

建议在 GitHub 仓库右侧 About 区域添加 topics：

```text
embedded-linux
v4l2
framebuffer
linux-input
camera
c
imx6u
```

这些关键词有助于面试官快速理解你的项目方向。

---

## 19. 后续修改代码的标准流程

以后你每改完一小部分，不要直接乱传。使用下面的固定流程。

查看状态：

```sh
git status
```

查看改了什么：

```sh
git diff
```

添加文件：

```sh
git add .
```

提交：

```sh
git commit -m "Add touch calibration support"
```

推送：

```sh
git push
```

一个好的 commit message 应该说明“做了什么”，例如：

```text
Add touch calibration support
Fix BMP row padding bug
Add YUYV to RGB565 conversion
Improve album image scaling
Document board deployment steps
```

不要写：

```text
update
fix
1
aaa
```

这些提交信息对找实习没有帮助。

---

## 20. 如何把这个项目写进简历

可以写成这样：

```text
嵌入式 Linux UI 相机项目

项目描述：
基于 V4L2、framebuffer 和 Linux input event 实现嵌入式开发板 UI 相机，支持实时预览、触摸操作、拍照、录像和相册浏览。

主要工作：
1. 使用 V4L2 mmap 缓冲队列完成摄像头 RGB565 图像采集。
2. 通过 framebuffer mmap 将摄像头帧直接显示到 LCD，降低额外拷贝和转换开销。
3. 基于 Linux input event 读取触摸屏事件，实现按钮命中判断和相机模式切换。
4. 实现 BMP 拍照保存、RGB565 原始帧录像和本地相册浏览。
5. 将工程模块化拆分为 lcd、v4l2、input、ui、photo、video、album，并编写完整学习文档。

技术栈：
C、Linux、V4L2、framebuffer、input event、Makefile、Git/GitHub
```

如果简历篇幅有限，可以压缩成：

```text
基于 V4L2 + framebuffer + input event 实现嵌入式 Linux UI 相机，支持实时预览、触摸拍照、RGB565 录像和 BMP 相册浏览；负责 V4L2 mmap 采集、LCD 显存显示、触摸事件处理、模块化工程组织和 GitHub 文档整理。
```

---

## 21. 面试时可以怎么讲

面试官可能会问：“你这个项目是怎么实现的？”

你可以按下面逻辑回答：

```text
这个项目运行在 Linux 开发板上，没有使用 Qt/OpenCV，而是直接操作 Linux 设备接口。

显示方面，我使用 /dev/fb0 framebuffer，通过 mmap 获得 LCD 显存地址，后续直接向显存写 RGB565 像素。

采集方面，我使用 V4L2 打开 /dev/videoX，设置摄像头输出 RGB565，并使用 VIDIOC_REQBUFS、VIDIOC_QUERYBUF、mmap、VIDIOC_QBUF、VIDIOC_DQBUF 构建采集队列。

主循环中，每次从 V4L2 队列取出一帧，把摄像头帧拷贝到 LCD，然后在同一块显存上绘制 UI 按钮。

触摸方面，我通过 /dev/input/eventX 读取 input_event，获取触摸坐标并缩放到 LCD 坐标系，再判断是否点击了拍照、录像、相册等按钮。

拍照时，我把当前 RGB565 摄像头帧转换成 24 位 BMP 保存；录像时，我把连续 RGB565 原始帧写入 .r565 文件；相册则扫描 photo 目录下的 BMP 文件并缩放显示。
```

这个回答能体现你理解系统整体链路，而不是只会运行代码。

---

## 22. 常见问题

### 22.1 `git push` 提示需要用户名密码

GitHub 现在不支持账号密码直接推送。解决方法：

1. 使用 SSH key。
2. 或使用 GitHub Personal Access Token。

推荐使用 SSH。

### 22.2 `fatal: remote origin already exists`

说明你已经添加过远程仓库。

查看：

```sh
git remote -v
```

如果地址错了，修改：

```sh
git remote set-url origin git@github.com:你的用户名/v4l2-ui-camera.git
```

### 22.3 `rejected` 或 `non-fast-forward`

常见原因：你在 GitHub 网页创建仓库时勾选了 README，导致远程仓库已经有一次提交。

如果你确认远程仓库只是刚创建的 README，可以执行：

```sh
git pull --rebase origin main
git push -u origin main
```

如果仍然冲突，建议删除 GitHub 仓库重新创建一个空仓库，再 push。

### 22.4 `.gitignore` 添加晚了，文件已经被 Git 跟踪

如果某个文件已经被提交，后来再加 `.gitignore` 不会自动取消跟踪。

例如取消跟踪 `obj/`：

```sh
git rm -r --cached obj
git commit -m "Remove build outputs from repository"
git push
```

注意：`--cached` 只是不再让 Git 跟踪，不会删除你本地文件。

### 22.5 不小心提交了大文件

如果还没有 push，可以回退最近一次提交：

```sh
git reset --soft HEAD~1
```

然后修改 `.gitignore`，重新提交。

如果已经 push 到 GitHub，处理会麻烦一些。对于刚创建的个人仓库，最简单的方法通常是删除仓库后重新创建并上传。

---

## 23. 推荐最终命令汇总

下面是最常用的一整套命令。把 `你的用户名` 换成你的 GitHub 用户名。

```sh
cd /path/to/v4l2_camera

git init
git config --global user.name "你的GitHub用户名"
git config --global user.email "你的GitHub邮箱"

git status --short
git add .
git commit -m "Release V1: embedded Linux V4L2 UI camera"
git branch -M main

git remote add origin git@github.com:你的用户名/v4l2-ui-camera.git
git push -u origin main

git tag -a v1.0.0 -m "V1: UI camera with preview, photo, recording and album"
git push origin v1.0.0
```

如果你使用 HTTPS，把远程地址改成：

```sh
git remote add origin https://github.com/你的用户名/v4l2-ui-camera.git
```

---

## 24. 上传成功后的检查清单

上传完成后，对照下面清单检查：

| 检查项 | 是否完成 |
| --- | --- |
| GitHub 仓库是 Public |  |
| 仓库名清晰，例如 `v4l2-ui-camera` |  |
| README 首页能正常显示 |  |
| `.gitignore` 已上传 |  |
| `obj/` 没有上传 |  |
| `.o` 文件没有上传 |  |
| `v4l2_test` 等二进制没有上传 |  |
| `docs/` 文档可以打开 |  |
| 已创建 `v1.0.0` tag |  |
| 已创建 GitHub Release |  |
| About 描述和 topics 已填写 |  |
| 简历中有 GitHub 项目链接 |  |

---

## 25. 一句话总结

你要上传的不是“一个能跑的文件”，而是一个能体现工程能力的作品集项目。

所以 GitHub V1 最重要的是：

```text
源码干净
结构清楚
README 完整
文档详细
版本明确
提交记录规范
```

做到这些，这个项目在实习投递时才真正有展示价值。
