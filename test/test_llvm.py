import os
import sys
import shutil
import datetime

# config
base_dir = os.path.dirname(os.path.abspath(__file__))
build_dir = os.path.join(base_dir, "test_build")
bin_dir = os.path.abspath(os.path.join(base_dir, "../bin"))
testdata_dir = os.path.join(base_dir, "test_data")
testdata_zip = os.path.join(testdata_dir, "testdata.zip")

BUILD_TYPE = "Release"
OPT_LEVEL = 0
TARGET_LANG = "llvm"

try:
    # 清理旧的构建目录（如果存在）
    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)

    # 创建并进入构建目录
    os.makedirs(build_dir, exist_ok=True)
    os.chdir(build_dir)

    # 执行构建命令
    cmake_cmd = f"cmake ../.. -D CMAKE_BUILD_TYPE={BUILD_TYPE}"
    make_cmd = "make -j4"
    
    if os.system(cmake_cmd) != 0:
        raise RuntimeError("CMake Configuration failed")
    
    if os.system(make_cmd) != 0:
        raise RuntimeError("compiler failed")

    # 验证编译器存在
    compiler_path = os.path.join(bin_dir, "compiler")
    if not os.path.exists(compiler_path):
        raise FileNotFoundError(f"Failed: {compiler_path}")

    print("Built compiler path: " + compiler_path)

    # 创建测试数据目录
    os.makedirs(testdata_dir, exist_ok=True)
    
    # 仅在文件不存在时下载
    if not os.path.exists(testdata_zip):
        print("Downloading testdata")
        os.chdir(testdata_dir)  # 切换到数据目录下载
        os.system("wget -O testdata.zip https://gitlab.eduxiji.net/csc1/nscscc/compiler2024/-/raw/main/testdata.zip")
        print("Downloading testdata complete")
    else:
        print("Testdata existed")
    
    # 解压测试数据到临时目录
    temp_dir = os.path.join(testdata_dir, "temp")
    os.makedirs(temp_dir, exist_ok=True)
    os.system(f"unzip -qo {testdata_zip} -d {temp_dir}")
    
    # 移动所有子文件夹到正确位置
    extracted_testdata_dir = os.path.join(temp_dir, "testdata")
    if os.path.exists(extracted_testdata_dir):
        # 遍历testdata目录中的所有内容
        for item in os.listdir(extracted_testdata_dir):
            source_path = os.path.join(extracted_testdata_dir, item)
            target_path = os.path.join(testdata_dir, item)
            
            # 如果目标已存在，先删除
            if os.path.exists(target_path):
                if os.path.isdir(target_path):
                    shutil.rmtree(target_path)
                else:
                    os.remove(target_path)
            
            # 移动文件或目录到目标位置
            shutil.move(source_path, target_path)
    else:
        print(f"Warning: Expected directory {extracted_testdata_dir} not found after extraction")
    
    # 清理临时目录
    shutil.rmtree(temp_dir)

    # 构建sysy标准库
    os.chdir(base_dir)
    os.system("clang -emit-llvm -S sylib.c -o lib.ll")

    subdirs = [entry.path for entry in os.scandir(testdata_dir) if entry.is_dir()]

    for subdir in subdirs:
        print(f"Processing test cases in: {subdir}")
        
        # 获取所有.sy测试文件
        sy_files = [f for f in os.listdir(subdir) if f.endswith(".sy")]
        sy_files.sort()
        
        for sy_file in sy_files:
            # 构造输入输出路径
            sy_path = os.path.join(subdir, sy_file)
            base_name = os.path.splitext(sy_file)[0]
            ll_path = os.path.join(subdir, f"{base_name}.ll")
            
            # 构建编译命令
            compile_cmd = (
                f"{compiler_path} {sy_path} "
                f"-O{OPT_LEVEL} "
                f"-emit-llvm {ll_path}"
            )
            
            # 执行编译命令
            print(f"Compiling: {sy_path} -> {ll_path}")
            exit_code = os.system(compile_cmd)
            
            if exit_code != 0:
                print(f"Fail on compile {sy_file}")
                raise RuntimeError("Fail on compile")

except Exception as e:
    print(f"Test Error: {str(e)}")
    sys.exit(1)

finally:
    # 清理构建目录
    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)
    # 切换回原始目录
    os.chdir(base_dir)
    os.system("rm lib.ll")
