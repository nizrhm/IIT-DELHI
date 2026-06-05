import sys
try:
    import PyPDF2
    def extract(path):
        with open(path, 'rb') as f:
            reader = PyPDF2.PdfReader(f)
            text = ""
            for p in reader.pages:
                text += p.extract_text() + "\n"
        return text
    print("PyPDF2")
    print(extract("Project Report.pdf")[:2000])
except Exception as e:
    try:
        import fitz
        def extract(path):
            doc = fitz.open(path)
            text = ""
            for p in doc:
                text += p.get_text() + "\n"
            return text
        print("fitz")
        print(extract("Project Report.pdf")[:2000])
    except Exception as e2:
        print("Error:", e, e2)
