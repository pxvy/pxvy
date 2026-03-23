import argparse
import os
import re
import tempfile


def smi_to_srt(smi_path, srt_path):
    """
    간단한 SMI → SRT 변환 함수
    (기본적인 SYNC 태그 기반 파싱)
    """
    with open(smi_path, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()

    # SYNC 태그 기준 분리
    sync_pattern = re.compile(r'<SYNC Start=(\d+)>', re.IGNORECASE)
    parts = sync_pattern.split(content)

    # parts 구조: [앞부분, time1, text1, time2, text2, ...]
    entries = []

    for i in range(1, len(parts), 2):
        start_time = int(parts[i])
        text = parts[i + 1]

        # 다음 sync가 있으면 end time 설정
        if i + 2 < len(parts):
            end_time = int(parts[i + 2])
        else:
            end_time = start_time + 2000  # fallback (2초)

        # HTML 태그 제거
        clean_text = re.sub(r'<.*?>', '', text).strip()
        clean_text = clean_text.replace('&nbsp;', ' ').strip()

        if clean_text:
            entries.append((start_time, end_time, clean_text))

    def ms_to_srt_time(ms):
        h = ms // 3600000
        ms %= 3600000
        m = ms // 60000
        ms %= 60000
        s = ms // 1000
        ms %= 1000
        return f"{h:02}:{m:02}:{s:02},{ms:03}"

    with open(srt_path, 'w', encoding='utf-8') as f:
        for idx, (start, end, text) in enumerate(entries, 1):
            f.write(f"{idx}\n")
            f.write(f"{ms_to_srt_time(start)} --> {ms_to_srt_time(end)}\n")
            f.write(f"{text}\n\n")


def main():
    parser = argparse.ArgumentParser(description="pytools")
    parser.add_argument("-s", "--smitosrt", required=True, help="SMI to SRT")

    args = parser.parse_args()

    video_path = args.smitosrt

    if not os.path.exists(video_path):
        return

    base, _ = os.path.splitext(video_path)
    srt_path = os.path.join(tempfile.gettempdir(), os.path.splitext(os.path.basename(base))[0]) + ".srt"
    # srt_path = base + ".srt"
    smi_path = base + ".smi"

    # if os.path.exists(srt_path):
    #     print(f"[INFO] 이미 SRT 존재: {srt_path}")
    #     return

    if os.path.exists(smi_path):
        print(srt_path)
        smi_to_srt(smi_path, srt_path)


if __name__ == "__main__":
    main()
