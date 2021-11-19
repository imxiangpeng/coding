# mxp, 20211105, frontpaneld module
# you should use the following method to include it
# $(call inherit-product, vendor/inspur/common/system/frontpaneld/frontpaneld.mk)


# mxp, 20211105, please see vendor/inspur/common/system/frontpaneld
# please also using custom gpio_keys_polled.kl & adc-keys.kl
PRODUCT_PACKAGES += frontpaneld

PRODUCT_COPY_FILES += \
  vendor/inspur/common/system/frontpaneld/gpio_keys_polled.kl:vendor/usr/keylayout/gpio_keys_polled.kl \
  vendor/inspur/common/system/frontpaneld/adc-keys.kl:vendor/usr/keylayout/adc-keys.kl
    

