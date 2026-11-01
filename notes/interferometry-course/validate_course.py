"""Выполнить тетради в настоящем IPython-ядре и сохранить результаты.

python notes/interferometry-course/validate_course.py [--only 00 01]
Рабочие файлы и отчёт проверки — в build/course-validation.
"""
import argparse
import json
import os
from pathlib import Path
import queue
import time
from jupyter_client import KernelManager

COURSE=Path(__file__).resolve().parent
WORK=COURSE.parents[1]/'build/course-validation'


def execute(path):
    notebook=json.loads(path.read_text(encoding='utf8'))
    assert notebook['nbformat']==4
    ids=[c['id'] for c in notebook['cells']]
    assert len(ids)==len(set(ids))
    env=os.environ.copy()
    env.update(IPYTHONDIR=str(WORK/'ipython'),JUPYTER_RUNTIME_DIR=str(WORK/'runtime'),
               JUPYTER_CONFIG_DIR=str(WORK/'jupyter'),MPLCONFIGDIR=str(WORK/'matplotlib'),
               MPLBACKEND='module://matplotlib_inline.backend_inline',PYTHONIOENCODING='utf-8')
    for folder in ['ipython','runtime','jupyter','matplotlib']:
        (WORK/folder).mkdir(parents=True,exist_ok=True)
    manager=KernelManager(connection_file=str(WORK/(path.stem+'-kernel.json')))
    manager.start_kernel(cwd=str(COURSE),env=env)
    client=manager.client()
    client.start_channels()
    start=time.monotonic()
    code_cells=0; figures=0
    try:
        client.wait_for_ready(timeout=60)
        for index,cell in enumerate(notebook['cells']):
            if cell['cell_type']!='code': continue
            source=''.join(cell['source'])
            cell['outputs']=[]
            msg_id=client.execute(source,store_history=True,stop_on_error=True)
            deadline=time.monotonic()+180
            while True:
                if time.monotonic()>deadline: raise TimeoutError(f'{path.name}, cell {index}')
                try: msg=client.get_iopub_msg(timeout=1)
                except queue.Empty: continue
                if msg.get('parent_header',{}).get('msg_id')!=msg_id: continue
                kind=msg['header']['msg_type']; data=msg['content']
                if kind=='execute_input': cell['execution_count']=data['execution_count']
                elif kind=='stream':
                    cell['outputs'].append(dict(output_type='stream',name=data['name'],text=data['text']))
                elif kind in ['display_data','execute_result']:
                    output=dict(output_type=kind,data=data['data'],metadata=data.get('metadata',{}))
                    if kind=='execute_result': output['execution_count']=data['execution_count']
                    cell['outputs'].append(output)
                    if 'image/png' in data['data']: figures+=1
                elif kind=='error':
                    raise RuntimeError(f'{path.name}, cell {index}: {data["ename"]}: {data["evalue"]}\n'+'\n'.join(data['traceback']))
                elif kind=='status' and data['execution_state']=='idle': break
            code_cells+=1
        notebook['metadata']['course']['validation']={'executed':True,'code_cells':code_cells,'figures':figures}
        path.write_text(json.dumps(notebook,ensure_ascii=False,indent=1)+'\n',encoding='utf8')
        return dict(file=path.name,code_cells=code_cells,figures=figures,seconds=round(time.monotonic()-start,2),status='passed')
    finally:
        client.stop_channels()
        manager.shutdown_kernel(now=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--only',nargs='*',help='Префиксы имён тетрадей, например 00 01')
    args=parser.parse_args()
    WORK.mkdir(parents=True,exist_ok=True)
    results=[]
    for path in sorted(COURSE.glob('*.ipynb')):
        if args.only and not any(path.name.startswith(prefix) for prefix in args.only): continue
        result=execute(path)
        results.append(result)
        print(json.dumps(result,ensure_ascii=False),flush=True)
    (WORK/'report.json').write_text(json.dumps(results,ensure_ascii=False,indent=2),encoding='utf8')
