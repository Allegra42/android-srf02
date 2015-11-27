# Adding the SRF02 proximity sensor to Android 

In this project, I add a proximity sensor to Android 4.4.4 running on a Pandaboard ES. To do so I had to write a kernel module and write parts of a HAL library. 
This project was developed during an internship at inovex GmbH. It should be considered work in progress with several ugly details that I know have to be fixed. And they will be in the future -- probably. 
Only those files I really had to change have been uploaded here. 

The project is based on Linaro Android 14.10. U-boot has been replaced by a newer version to get it running on both Pandaboard ES Rev. B2 and B3. 
The sensor I used was a SRF02 connected via the I2C interface.







### Download the Linaro 14.10 Android sources
``` 
export MANIFEST_REPO=git://android.git.linaro.org/platform/manifest.git
export MANIFEST_BRANCH=linaro-android-14.10-release
export MANIFEST_FILENAME=panda-linaro.xml

repo init -u ${MANIFEST_REPO} -b ${MANIFEST_BRANCH} -m ${MANIFEST_FILENAME} --repo-url=git://android.git.linaro.org/tools/repo -g common,pandaboard

// Change broken linaro manifest
rm .repo/manifest.xml
cp fixed_manifest.xml .repo/manifest.xml

repo sync
```

### Build
```
./build_linaro.sh
```
OR
```
source build/envsetup.sh
lunch pandaboard-eng
export WITH_HOST_DALVIK=false
make -j4 boottarball systemtarball userdatatarball
```
