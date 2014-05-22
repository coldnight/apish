## apish

### 介绍
由于工作中有很多 API 需要测试, 写实例太麻烦, 就写了个工具用来交互式的测试 API.

#### 为什么用 C
由于最近刚刚看完 《C和指针》 这本书, 为了巩固学习成果就将之前一个用 Python 写的项目
用 C 改写了下, 仅仅支持 POST/GET 请求. 可以在参数里引用其他请求返回的 JSON 数据.

### 编译
依赖库:
* libcurl
* readline
* [json-c](https://github.com/json-c/json-c)

处理好依赖后可以使用 make 命令构建
```bash
make
```

### 特性
主要特性就是可以在一个请求里引用别的请求返回的 JSON 数据, 支持一下格式
```
{{ 0.data.uid }}        // 第一个请求返回的数据的 data 字段里面的 uid 字段
{{ 0["data"]["uid"] }}  // 和上面一致
{{ 0["data"].uid }}     // 和上面一致
{{ 0.data.array[1] }}   // 支持数组访问
```

每个界面都包含一个 help 命令, 可以用 help 查看帮助.

最后放出一个[终端录屏](https://asciinema.org/a/9716)
