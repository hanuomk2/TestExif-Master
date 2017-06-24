#include "widget.h"
#include "ui_widget.h"
extern "C" {
    #include "exif-master/exif.h"
}

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget)
{
    ui->setupUi(this);
    //lineEditの初期化
    ui->lineEdit->setText(PATH);

    //plainTextEditの初期化
    ui->plainTextEdit->setReadOnly(true);

    //grahicsViewの初期化
    ui->graphicsView->setScene(&scene);

    //各ボタンのアイコン画像の設定
    ui->btnFileDialog->setIcon(QIcon(QApplication::style()->standardIcon(QStyle::SP_DirIcon)));
    ui->btnEnterKey->setIcon(QIcon(QApplication::style()->standardIcon(QStyle::SP_DialogOkButton)));

    //シグナル・スロットの設定
    //各ボタンのシグナル・スロットを設定
    connect(ui->btnFileDialog, SIGNAL(clicked(bool)), this, SLOT(openFileDialog()));
    connect(ui->btnEnterKey, SIGNAL(clicked(bool)), this, SLOT(readJpegFile()));

    //lineEditで[Enter]キー押下時のシグナル・スロットを設定
    connect(ui->lineEdit, SIGNAL(returnPressed()), this, SLOT(readJpegFile()));
}

Widget::~Widget()
{
    delete ui;
}

//[ファイルダイアログを開く]ボタンが押下された場合
void Widget::openFileDialog()
{
    // ファイルダイアログを表示
    QString strPATH = QFileDialog::getOpenFileName(this, tr("Choose Jpeg File"), PATH, tr("images (*.jpg *.jpeg)"));

    // ファイルダイアログで正常な戻り値が出力された場合
    if(!strPATH.isEmpty()){
        ui->lineEdit->setText(strPATH);
        readJpegFile();
    }
}

void Widget::readJpegFile()
{
    //構造体、変数等の初期化
    void **ifdArray;
    TagNodeInfo *tag;
    int i, result;


    // plainTextEditをクリアする
    ui->plainTextEdit->clear();
    // GraphicsViewをクリアする
    scene.clear();

    // lineEditに入力したファイル名のbaseNameを取得
    QString strImagename = ui->lineEdit->text();
    QFileInfo info(strImagename);
    QString strBasename = info.completeBaseName();

    /* lineEditに入力したファイル名がjpg or jpegで、実際に存在するか確認 */
    QString strDirname = info.absolutePath();
    QDir dir(strDirname);
    QStringList listFilter;
    listFilter << "*.jpg" << "*.jpeg";
    QStringList listImage = dir.entryList(listFilter);

    // lineEditに入力したファイル名が、listImage内に実際に存在するかをfor文で探索
    for(i=0; i< listImage.size(); i++){
        QFileInfo info2(listImage.at(i));
         if(strBasename == info2.baseName()){
             /* lineEditに入力したファイル名がjpg or jpegで、実際に存在した場合の処理 *****************/
             // QStringをchar*に変換
             std::string cStr = strImagename.toLocal8Bit();
             unsigned int len = cStr.length();
             char* image_full_path = new char[len+1];
             memcpy(image_full_path, cStr.c_str(), len+1);

             /* IFDテーブルの作成 *********************************************************************/
             // parse the JPEG header and create the pointer array of the IFD tables
             ifdArray = createIfdTableArray(image_full_path, &result);

             // check result status
             switch (result) {
             case 0: // no IFDs
                 qDebug("[%s] does not seem to contain the Exif segment.\n", image_full_path);
                 break;
             case ERR_READ_FILE:
                 qDebug("failed to open or read [%s].\n", image_full_path);
                 break;
             case ERR_INVALID_JPEG:
                 qDebug("[%s] is not a valid JPEG file.\n", image_full_path);
                 break;
             case ERR_INVALID_APP1HEADER:
                 qDebug("[%s] does not have valid Exif segment header.\n", image_full_path);
                 break;
             case ERR_INVALID_IFD:
                 qDebug("[%s] contains one or more IFD errors. use -v for details.\n", image_full_path);
                 break;
             default:
                 qDebug("[%s] createIfdTableArray: result=%d\n", image_full_path, result);
                 break;
             }
             if (!ifdArray) return;

             /* IFDテーブルをダンプして、Exifタグ等をplainTextEditに出力する ***************************/
             // dump all IFD tables
             for (i = 0; ifdArray[i] != NULL; i++) {
                dumpIfdTableToPlainTextEdit(ifdArray[i]);
             }
             // or dumpIfdTableArray(ifdArray);

              /* 1st IFDにサムネイル画像があれば、それを表示する **************************************/
              unsigned char *p;
              int result;

              // try to get thumbnail data from 1st IFD
              p = getThumbnailDataOnIfdTableArray(ifdArray, &len, &result);
              if (p) {
                  //サムネイルデータをJPGフォーマットでQImageに流し込む
                  QImage image;
                  image.loadFromData(p, len, "JPG");

                  /* TAG_Orientationを基に画像の向きを正す**************************************/
                  tag = getTagInfo(ifdArray, IFD_0TH, TAG_Orientation);
                  if (tag) {
                      if (!tag->error) {
                          //反転処理
                          switch (tag->numData[0]) {
                              case 2: case 5: case 7:
                                  // 水平反転
                                  image = image.mirrored(true, false);
                                  break;
                              case 4:
                                  // 垂直反転
                                  image = image.mirrored(false, true);
                                  break;
                              default:
                                  break;
                          }

                          // 回転処理
                          switch (tag->numData[0]) {
                              case 3:
                                  image = image.transformed(QMatrix().rotate(180));
                                  break;
                              case 6: case 7:
                                  image = image.transformed(QMatrix().rotate(90));
                                  break;
                              case 5: case 8:
                                  image = image.transformed(QMatrix().rotate(270));
                                  break;
                              default:
                                  break;
                          }
                      }
                      freeTagInfo(tag);
                  }

                  // 描画処理(QImageをQPixmapに変換)
                  QPixmap pixmap = QPixmap::fromImage(image);
                  scene.addPixmap(pixmap);

                  // free IFD table array
                  freeIfdTableArray(ifdArray);
                  break;
              }
              else {
                  qDebug("getThumbnailDataOnIfdTableArray: ret=%d\n", result);

                  // free IFD table array
                  freeIfdTableArray(ifdArray);
                  break;
              }
         }
    }
}

void Widget::dumpIfdTableToPlainTextEdit(void *pIfd)
{
    int i;
    IfdTable *ifd;
    TagNode *tag;
    int cnt = 0;
    unsigned int count;

    if (!pIfd) {
        return;
    }
    ifd = (IfdTable*)pIfd;

    QString str;

    str.sprintf("{%s IFD}",
        (ifd->ifdType == IFD_0TH)  ? "0TH" :
        (ifd->ifdType == IFD_1ST)  ? "1ST" :
        (ifd->ifdType == IFD_EXIF) ? "EXIF" :
        (ifd->ifdType == IFD_GPS)  ? "GPS" :
        (ifd->ifdType == IFD_IO)   ? "Interoperability" : "");
    ui->plainTextEdit->insertPlainText(str);

    str.sprintf(" tags=%u\n", ifd->tagCount);
    ui->plainTextEdit->insertPlainText(str);

    tag = ifd->tags;
    while (tag) {
        str.sprintf("tag[%02d] 0x%04X %s\n", cnt++, tag->tagId, getTagName(ifd->ifdType, tag->tagId));
        ui->plainTextEdit->insertPlainText(str);

        str.sprintf("\ttype=%u count=%u ", tag->type, tag->count);
        ui->plainTextEdit->insertPlainText(str);

        str.sprintf("val=");
        ui->plainTextEdit->insertPlainText(str);

        if (tag->error) {
            str.sprintf("(error)");
            ui->plainTextEdit->insertPlainText(str);
        } else {
            switch (tag->type) {
            case TYPE_BYTE:
                for (i = 0; i < (int)tag->count; i++) {
                    str.sprintf("%u ", (unsigned char)tag->numData[i]);
                    ui->plainTextEdit->insertPlainText(str);
                }
                break;

            case TYPE_ASCII:
                str.sprintf("[%s]", (char*)tag->byteData);
                ui->plainTextEdit->insertPlainText(str);
                break;

            case TYPE_SHORT:
                for (i = 0; i < (int)tag->count; i++) {
                    str.sprintf("%hu ", (unsigned short)tag->numData[i]);
                    ui->plainTextEdit->insertPlainText(str);
                }
                break;

            case TYPE_LONG:
                for (i = 0; i < (int)tag->count; i++) {
                    str.sprintf("%u ", tag->numData[i]);
                    ui->plainTextEdit->insertPlainText(str);
                }
                break;

            case TYPE_RATIONAL:
                for (i = 0; i < (int)tag->count; i++) {
                    str.sprintf("%u/%u ", tag->numData[i*2], tag->numData[i*2+1]);
                    ui->plainTextEdit->insertPlainText(str);
                }
                break;

            case TYPE_SBYTE:
                for (i = 0; i < (int)tag->count; i++) {
                    str.sprintf("%d ", (char)tag->numData[i]);
                    ui->plainTextEdit->insertPlainText(str);
                }
                break;

            case TYPE_UNDEFINED:
                count = tag->count;
                // omit too long data
                if (count > 16) { // && !Verbose) {
                    count = 16;
                }
                for (i = 0; i < (int)count; i++) {
                    // if character is printable
                    if (isgraph(tag->byteData[i])) {
                        str.sprintf("%c ", tag->byteData[i]);
                        ui->plainTextEdit->insertPlainText(str);
                    } else {
                        str.sprintf("0x%02x ", tag->byteData[i]);
                        ui->plainTextEdit->insertPlainText(str);
                    }
                }
                if (count < tag->count) {
                    str.sprintf("(omitted)");
                    ui->plainTextEdit->insertPlainText(str);
                }
                break;

            case TYPE_SSHORT:
                for (i = 0; i < (int)tag->count; i++) {
                    str.sprintf("%hd ", (short)tag->numData[i]);
                    ui->plainTextEdit->insertPlainText(str);
                }
                break;

            case TYPE_SLONG:
                for (i = 0; i < (int)tag->count; i++) {
                    str.sprintf("%d ", (int)tag->numData[i]);
                    ui->plainTextEdit->insertPlainText(str);
                }
                break;

            case TYPE_SRATIONAL:
                for (i = 0; i < (int)tag->count; i++) {
                    str.sprintf("%d/%d ", (int)tag->numData[i*2], (int)tag->numData[i*2+1]);
                    ui->plainTextEdit->insertPlainText(str);
                }
                break;

            default:
                break;
            }
        }
        str.sprintf("\n");
        ui->plainTextEdit->insertPlainText(str);

        tag = tag->next;
    }
    str.sprintf("\n");
    ui->plainTextEdit->insertPlainText(str);

    return;
}

char *Widget::getTagName(int ifdType, unsigned short tagId)
{
    static char tagName[128];
    if (ifdType == IFD_0TH || ifdType == IFD_1ST || ifdType == IFD_EXIF) {
        strcpy(tagName,
            (tagId == 0x0100) ? "ImageWidth" :
            (tagId == 0x0101) ? "ImageLength" :
            (tagId == 0x0102) ? "BitsPerSample" :
            (tagId == 0x0103) ? "Compression" :
            (tagId == 0x0106) ? "PhotometricInterpretation" :
            (tagId == 0x0112) ? "Orientation" :
            (tagId == 0x0115) ? "SamplesPerPixel" :
            (tagId == 0x011C) ? "PlanarConfiguration" :
            (tagId == 0x0212) ? "YCbCrSubSampling" :
            (tagId == 0x0213) ? "YCbCrPositioning" :
            (tagId == 0x011A) ? "XResolution" :
            (tagId == 0x011B) ? "YResolution" :
            (tagId == 0x0128) ? "ResolutionUnit" :

            (tagId == 0x0111) ? "StripOffsets" :
            (tagId == 0x0116) ? "RowsPerStrip" :
            (tagId == 0x0117) ? "StripByteCounts" :
            (tagId == 0x0201) ? "JPEGInterchangeFormat" :
            (tagId == 0x0202) ? "JPEGInterchangeFormatLength" :

            (tagId == 0x012D) ? "TransferFunction" :
            (tagId == 0x013E) ? "WhitePoint" :
            (tagId == 0x013F) ? "PrimaryChromaticities" :
            (tagId == 0x0211) ? "YCbCrCoefficients" :
            (tagId == 0x0214) ? "ReferenceBlackWhite" :

            (tagId == 0x0132) ? "DateTime" :
            (tagId == 0x010E) ? "ImageDescription" :
            (tagId == 0x010F) ? "Make" :
            (tagId == 0x0110) ? "Model" :
            (tagId == 0x0131) ? "Software" :
            (tagId == 0x013B) ? "Artist" :
            (tagId == 0x8298) ? "Copyright" :
            (tagId == 0x8769) ? "ExifIFDPointer" :
            (tagId == 0x8825) ? "GPSInfoIFDPointer":
            (tagId == 0xA005) ? "InteroperabilityIFDPointer" :

            (tagId == 0x4746) ? "Rating" :

            (tagId == 0x9000) ? "ExifVersion" :
            (tagId == 0xA000) ? "FlashPixVersion" :

            (tagId == 0xA001) ? "ColorSpace" :

            (tagId == 0x9101) ? "ComponentsConfiguration" :
            (tagId == 0x9102) ? "CompressedBitsPerPixel" :
            (tagId == 0xA002) ? "PixelXDimension" :
            (tagId == 0xA003) ? "PixelYDimension" :

            (tagId == 0x927C) ? "MakerNote" :
            (tagId == 0x9286) ? "UserComment" :

            (tagId == 0xA004) ? "RelatedSoundFile" :

            (tagId == 0x9003) ? "DateTimeOriginal" :
            (tagId == 0x9004) ? "DateTimeDigitized" :
            (tagId == 0x9290) ? "SubSecTime" :
            (tagId == 0x9291) ? "SubSecTimeOriginal" :
            (tagId == 0x9292) ? "SubSecTimeDigitized" :

            (tagId == 0x829A) ? "ExposureTime" :
            (tagId == 0x829D) ? "FNumber" :
            (tagId == 0x8822) ? "ExposureProgram" :
            (tagId == 0x8824) ? "SpectralSensitivity" :
            (tagId == 0x8827) ? "PhotographicSensitivity" :
            (tagId == 0x8828) ? "OECF" :
            (tagId == 0x8830) ? "SensitivityType" :
            (tagId == 0x8831) ? "StandardOutputSensitivity" :
            (tagId == 0x8832) ? "RecommendedExposureIndex" :
            (tagId == 0x8833) ? "ISOSpeed" :
            (tagId == 0x8834) ? "ISOSpeedLatitudeyyy" :
            (tagId == 0x8835) ? "ISOSpeedLatitudezzz" :

            (tagId == 0x9201) ? "ShutterSpeedValue" :
            (tagId == 0x9202) ? "ApertureValue" :
            (tagId == 0x9203) ? "BrightnessValue" :
            (tagId == 0x9204) ? "ExposureBiasValue" :
            (tagId == 0x9205) ? "MaxApertureValue" :
            (tagId == 0x9206) ? "SubjectDistance" :
            (tagId == 0x9207) ? "MeteringMode" :
            (tagId == 0x9208) ? "LightSource" :
            (tagId == 0x9209) ? "Flash" :
            (tagId == 0x920A) ? "FocalLength" :
            (tagId == 0x9214) ? "SubjectArea" :
            (tagId == 0xA20B) ? "FlashEnergy" :
            (tagId == 0xA20C) ? "SpatialFrequencyResponse" :
            (tagId == 0xA20E) ? "FocalPlaneXResolution" :
            (tagId == 0xA20F) ? "FocalPlaneYResolution" :
            (tagId == 0xA210) ? "FocalPlaneResolutionUnit" :
            (tagId == 0xA214) ? "SubjectLocation" :
            (tagId == 0xA215) ? "ExposureIndex" :
            (tagId == 0xA217) ? "SensingMethod" :
            (tagId == 0xA300) ? "FileSource" :
            (tagId == 0xA301) ? "SceneType" :
            (tagId == 0xA302) ? "CFAPattern" :

            (tagId == 0xA401) ? "CustomRendered" :
            (tagId == 0xA402) ? "ExposureMode" :
            (tagId == 0xA403) ? "WhiteBalance" :
            (tagId == 0xA404) ? "DigitalZoomRatio" :
            (tagId == 0xA405) ? "FocalLengthIn35mmFormat" :
            (tagId == 0xA406) ? "SceneCaptureType" :
            (tagId == 0xA407) ? "GainControl" :
            (tagId == 0xA408) ? "Contrast" :
            (tagId == 0xA409) ? "Saturation" :
            (tagId == 0xA40A) ? "Sharpness" :
            (tagId == 0xA40B) ? "DeviceSettingDescription" :
            (tagId == 0xA40C) ? "SubjectDistanceRange" :

            (tagId == 0xA420) ? "ImageUniqueID" :
            (tagId == 0xA430) ? "CameraOwnerName" :
            (tagId == 0xA431) ? "BodySerialNumber" :
            (tagId == 0xA432) ? "LensSpecification" :
            (tagId == 0xA433) ? "LensMake" :
            (tagId == 0xA434) ? "LensModel" :
            (tagId == 0xA435) ? "LensSerialNumber" :
            (tagId == 0xA500) ? "Gamma" :
            "(unknown)");
    } else if (ifdType == IFD_GPS) {
        strcpy(tagName,
            (tagId == 0x0000) ? "GPSVersionID" :
            (tagId == 0x0001) ? "GPSLatitudeRef" :
            (tagId == 0x0002) ? "GPSLatitude" :
            (tagId == 0x0003) ? "GPSLongitudeRef" :
            (tagId == 0x0004) ? "GPSLongitude" :
            (tagId == 0x0005) ? "GPSAltitudeRef" :
            (tagId == 0x0006) ? "GPSAltitude" :
            (tagId == 0x0007) ? "GPSTimeStamp" :
            (tagId == 0x0008) ? "GPSSatellites" :
            (tagId == 0x0009) ? "GPSStatus" :
            (tagId == 0x000A) ? "GPSMeasureMode" :
            (tagId == 0x000B) ? "GPSDOP" :
            (tagId == 0x000C) ? "GPSSpeedRef" :
            (tagId == 0x000D) ? "GPSSpeed" :
            (tagId == 0x000E) ? "GPSTrackRef" :
            (tagId == 0x000F) ? "GPSTrack" :
            (tagId == 0x0010) ? "GPSImgDirectionRef" :
            (tagId == 0x0011) ? "GPSImgDirection" :
            (tagId == 0x0012) ? "GPSMapDatum" :
            (tagId == 0x0013) ? "GPSDestLatitudeRef" :
            (tagId == 0x0014) ? "GPSDestLatitude" :
            (tagId == 0x0015) ? "GPSDestLongitudeRef" :
            (tagId == 0x0016) ? "GPSDestLongitude" :
            (tagId == 0x0017) ? "GPSBearingRef" :
            (tagId == 0x0018) ? "GPSBearing" :
            (tagId == 0x0019) ? "GPSDestDistanceRef" :
            (tagId == 0x001A) ? "GPSDestDistance" :
            (tagId == 0x001B) ? "GPSProcessingMethod" :
            (tagId == 0x001C) ? "GPSAreaInformation" :
            (tagId == 0x001D) ? "GPSDateStamp" :
            (tagId == 0x001E) ? "GPSDifferential" :
            (tagId == 0x001F) ? "GPSHPositioningError" :
            "(unknown)");
    } else if (ifdType == IFD_IO) {
        strcpy(tagName,
            (tagId == 0x0001) ? "InteroperabilityIndex" :
            (tagId == 0x0002) ? "InteroperabilityVersion" :
            "(unknown)");
    }
    return tagName;
}
