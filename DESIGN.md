一、类的设计与职责划分
1.SomeObj 命令门面类
作为用户命令的直接接收者，负责解析参数，并调用底层的 Repository 类完成具体的任务。

1.Repository 仓库类
负责所有与 .gitlite 文件夹有关的底层 I/O 操作。封装了文件读写。

3.Commit 提交数据类
负责将提交信息 (Tree,Parent,Time,Message) 序列化为字符串，或者从字符串中反序列化出对象。

4.Utils 工具类
这是已经给出的类。
特别的，我给这里面多写了一个工具，trimTrailingNewlines: 用于删除字符串末的 '\n'/'\r'。

二、类的工作原理 


三、持久化实现
1.目录结构
.gitlite/objects/: 存放所有对象 (Blob,Tree,Commit)。文件名为其内容的哈希值。由于哈希值唯一，天然去重。
.gitlite/refs/heads: 存放分支引用，文件名是分支名
.gitlite/refs/tags: 存放标签引用，文件名是标签名

2.关键文件的序列化格式
HEAD: ref: refs/heads/master,永远指向当前活跃分支的路径。
refs/heads/<branch>: 内容为一行 40 位的哈希。
index 暂存区: 每行格式为 <'A'/'R'> <哈希值> <文件名>。每次 add 或 commit 直接覆盖该文件。
Tree 对象: 每行格式为 <Blob 哈希> <文件名>。
Commit 对象: 多行文本结构，包含: 
tree <Tree哈希>
parent <父提交哈希>
time <时间戳>
message <提交信息>
