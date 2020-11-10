![MarmaraCreditLoops Logo](https://raw.githubusercontent.com/marmarachain/marmara/master/MCL-Logo.png "Marmara Credit Loops Logo")

# Marmara v.1.0.1 Kurulum

Marmara Kredi Döngüleri Zeki kontrak blokzincirini kurmanın iki yolu bulunmaktadır.
## Önderlenmiş Marmara Kredi Döngüleri Blokzincirinin Kurulumu

Marmara Kredi Döngüleri blokzincirinin önceden derlenmiş halini indirip, zip halinde olan dosyayı açıp kurulumu hızlıca tamamlayabilirisiniz.

Bunun için aşağıdaki linkten ilgili dosyalara erişim sağlayabilirsiniz.

**__TO-DO: Add a link to simple installations section for pre-compiled executables of MCL__**

## Marmara Kredi Döngüleri Blokzincirinin Kaynak Kodundan Kurulumu

Marmara Kredi Döngüleri blokzinciri kaynak kodundan kurulabilir. Bu zorunlu bir işlem değildir (önceden derlenmiş hali de kullanılabilir), ancak kaynaktan derleme yöntemi kişinin en son yapılan yazılım yamalarına ve yükseltmelerine anında güncelleme yapmasına izin vermesi nedeniyle en iyi bir uygulama olarak kabul edilmektedir.
### Linux İşletim Sistemi

#### Kurulum için Gereksinimler

    Linux (Ubuntu gibi Debian tabanlı dağıtımlar)
        Ubuntu için 16.04 veya 18.04 sürümünün kullanımı önerilir

    64-bit İşlemci

    En az 2 CPU 

    Minimum 4GB Boş RAM (Önerilen 8 GB RAM'dir)

#### Başlangıç
Sistemin güncel olduğundan emin olunur:
```	
sudo apt-get update
sudo apt-get upgrade -y
```

#### Bağımlılık Paketlerinin İndirilmesi

```	
sudo apt-get install build-essential pkg-config libc6-dev m4 g++-multilib autoconf libtool ncurses-dev unzip git python zlib1g-dev wget bsdmainutils automake libboost-all-dev libssl-dev libprotobuf-dev protobuf-compiler libgtest-dev libqt4-dev libqrencode-dev libsodium-dev libdb++-dev ntp ntpdate software-properties-common curl clang libcurl4-gnutls-dev cmake clang -y
```
Bu işlem, İnternet bağlantınıza bağlı olarak biraz zaman alabilir. İşlemin arka planda çalışmasına izin veriniz.

Tamamlandığında, Marmara Kredi Döngüleri blokzinirini kurmak için aşağıdaki adımları izleyiniz.

#### Marmara Kredi Döngüleri Reposunun Klonlanması
```	
cd ~
git clone https://github.com/marmarachain/marmara komodo --branch master --single-branch
```
>8 GB RAM veya üstüne sahipseniz aşağıdaki adımları atlayarak **__Zcash Parametrelerinin Çekilmesi__** adımına geçiniz.

#### Swap(Takas) Alanının 4 GB şeklinde ayarlanması
Swap (Takas) Alanı, işletim sistemi tarafından sabit diskinizde ayrılmış olan bir bölümdür.
İşlenecek veriler ön belleğe (RAM) sığmadığı zaman bu bölüm “RAM”  gibi kullanılır ve böylelikle veri akışının ve proseslerinin devam etmesi sağlanır.  
Eğer kullanılan sistemde 4 GB'lik RAM mevcutsa, RAM boyutunun tamamen dolup, cevap verememesi durumuna karşılık bu alanın ayarlanması tavsiye edilir.

> Swap alanının olup olmadığını ``` sudo swapon --show ``` komutu ile kontrol edebilirsiniz.
Sistemde swapfile mevcut olup en az 4 GB olacak şekilde ayarlanmadıysa bu durumda öncelik olarak ```sudo swapoff /swapfile ``` komutuyla swap'ı kapatınız.

Swap alanını 4 GB şeklinde ayarlamak için aşağıdaki komutları komut satırına ekleyiniz: 
```
sudo fallocate -l 4G /swapfile
sudo chmod 600 /swapfile 
sudo mkswap /swapfile 
sudo swapon /swapfile
```
#### Zcash Parametrelerinin Çekilmesi
```
cd komodo
./zcutil/fetch-params.sh
./zcutil/build.sh -j$(nproc)
```
**Aşağıdaki adımlar kurulum için zorunlu olmayıp kullanıcının seçimine bırakılmıştır.**

##### Swappiness Ayarının Yapılması
Swappiness, Linux çekirdeğinin RAM içeriğini takas için ne kadar (ve ne sıklıkla) kopyaladığını tanımlayan çekirdek parametresidir. swappiness 0 ile 100 arasında bir değere sahip olabilir.
Aşağıda verilen komut, bu parametrenin değerini "10" şeklinde ayarlar. Swappiness parametresinin değeri ne kadar yüksekse, çekirdeğe işlemleri fiziksel bellekten agresif bir şekilde takas etmesini ve önbellek takas ettirmesini bildirecektir.
```
sudo sysctl vm.swappiness=10 
```
>Bu ayar, bir sonraki yeniden başlatmaya kadar geçerli olacaktır. Yeniden başlatma durumunda geçerli olmasını sağlamak amacıyla aşağıdaki komutu /etc/sysctl.conf dosyasına ekleyerek yeniden başlatımda otomatik olarak geçerli olmasını sağlayabiliriz:
>```sudo nano /etc/sysctl.conf``` vm.swappiness=10

#### UFW Ayarının Yapılması
Bu adım MCL kurulumu için zorunlu olmayıp makinanın veya sunucunun güvenliği sağlamak amacıyla kullanılabilir.

- UFW nin statüsü aşağıdaki komut ile kontrol edilir:
```
sudo ufw status
```
- Eğer UFW paketi yüklü değilse aşağıdaki komut kullanılarak indirilir:
```	
sudo apt install ufw
```
- Bağlantıların yapılmasını sağlayacak şekilde aşağıdaki komutlarla UFW etkinleştirilir:
```
echo y | sudo ufw enable
sudo ufw allow ssh
sudo ufw allow "OpenSSH"
sudo ufw allow 33824
```
# Önemli Bilgilendirme
İşbu yazılım **Komodo** üzerine, *Komodo* yazılımı da **zcash** üzerine geliştirilmiş olup sürekli olarak yoğun geliştirme çalışmaları altındadır.
**Marmara Kredi Döngüleri eksperimental olup sürekli geliştirme fazındadır.** Kendi sorumluluğunuzda kullanınız.

MCL ve Komodo Platform geliştiricileri hiçbir zaman ve hiç bir koşulda işbu yazılımın kullanımından kaynaklanan herhangi bir olası zarardan sorumlu değildir.
	
# Lisans
Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
