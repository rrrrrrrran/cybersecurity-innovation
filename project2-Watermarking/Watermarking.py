import numpy as np
import cv2
import matplotlib.pyplot as plt
def embed_watermark(image, watermark, alpha=10):
    img = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
    wm = cv2.resize(watermark, (img.shape[1] // 8, img.shape[0] // 8))
    wm = (wm > 128).astype(np.float32)

    img_dct = cv2.dct(np.float32(img))
    h, w = wm.shape
    img_dct[0:h, 0:w] += alpha * wm
    img_watermarked = cv2.idct(img_dct)
    return np.clip(img_watermarked, 0, 255).astype(np.uint8)

def extract_watermark(watermarked, original, wm_shape, alpha=10):
    wm_h, wm_w = wm_shape
    wm_dct_w = cv2.dct(np.float32(watermarked))
    wm_dct_o = cv2.dct(np.float32(original))
    wm_extracted = (wm_dct_w[0:wm_h, 0:wm_w] - wm_dct_o[0:wm_h, 0:wm_w]) / alpha
    return np.clip(wm_extracted * 255, 0, 255).astype(np.uint8)

def flip(img):
    return cv2.flip(img, 1)  # 水平翻转

def shift(img, dx=20, dy=20):
    rows, cols = img.shape
    M = np.float32([[1, 0, dx], [0, 1, dy]])
    return cv2.warpAffine(img, M, (cols, rows))

def crop(img, percent=0.8):
    h, w = img.shape
    new_h, new_w = int(h * percent), int(w * percent)
    return img[0:new_h, 0:new_w]

img = cv2.imread("lena.png")
wm = cv2.imread("wm.png", 0)

# 嵌入水印
watermarked = embed_watermark(img, wm)
cv2.imwrite("watermarked.png", watermarked)

# 攻击测试
attacked_flip = flip(watermarked)

cv2.imwrite("flipped.png", attacked_flip)

# 提取水印
wm_extracted_flip = extract_watermark(attacked_flip, cv2.cvtColor(img, cv2.COLOR_BGR2GRAY), (wm.shape[0], wm.shape[1]))

# 显示
plt.subplot(1, 2, 1)
plt.title("Original WM")
plt.imshow(wm, cmap='gray')
plt.subplot(1, 2, 2)
plt.title("Extracted WM with flip attack")
plt.imshow(wm_extracted_flip, cmap='gray')
plt.show()
