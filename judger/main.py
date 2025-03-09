import subprocess
import os
import threading
import sys
import logging

def load_data():
    testdata = []
    types = os.listdir('testdata')
    for type in types:
        if os.path.isdir('testdata/' + type) and type != '.git':
            points = os.listdir('testdata/' + type)
            for point in points:
                if os.path.isdir('testdata/' + type + '/' + point):
                    if os.path.exists('testdata/' + type + '/' + point + '/' + point + '.in'):
                        testdata.append((
                            'testdata/' + type + '/' + point,
                            'testdata/' + type + '/' + point + '/' + point + '.in',
                            'testdata/' + type + '/' + point + '/' + point
                        ))
                    else:
                        testdata.append((
                            'testdata/' + type + '/' + point,
                            None,
                            'testdata/' + type + '/' + point + '/' + point
                        ))
    return testdata


def run_compile(compiler, testpoint):
    testdir = testpoint[0]
    inpath = testpoint[1]
    point_name = testpoint[2]
    logging.info(f"Compiling {point_name}.sy...")
    try:
        res = subprocess.run(
            [
                compiler,
                point_name + '.sy',
                '-emit-llvm',
                point_name + '.ll',
                '-O0',
                ],
            capture_output=True,
            text=True,
            timeout=120,
        )
        if not res.stderr is None:
            logging.error(f"Error when compiling {point_name}.sy:\n==>{res.stderr}")
            return
    except subprocess.TimeoutExpired:
        logging.error(f"Compile {point_name}.sy timeout.")
    except subprocess.CalledProcessError as e:
        logging.error(f"Compile {point_name}.sy failed with exit code {e.returncode}:\n==>{e.stderr}")
        return
    except Exception as e:
        logging.error(f"An unexpected error occurred during compilation of {point_name}.sy: {type(e).__name__}: {e}")
        return


if __name__ == '__main__':
    testdata = load_data()
    logging.basicConfig(level=logging.INFO)
    logging.info('Testpoints loaded.')
    compiler = os.path.abspath(
        'bin/compiler.exe' if sys.platform == 'win32' else 'bin/compiler'
    )
    threads = []
    os.system('clang -emit-llvm -c libsysy/libsysy.c -S -o libsysy/libsysy.ll')
    for testpoint in testdata:
        run_compile(compiler, testpoint)
    # for testpoint in testdata:
    #     thread = threading.Thread(target=run_compile, args=(compiler, testpoint,))
    #     threads.append(thread)
    #     thread.start()
    # for thread in threads:
    #     thread.join()
    logging.info('All testpoints finished.')