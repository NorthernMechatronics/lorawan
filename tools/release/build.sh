#!/bin/sh

declare -a bsp_list=(
  "BSP_NM180100EVB"
  "BSP_NM180410"
  "BSP_NM180411"
)

variant="Release"
f_lorawan=false
f_ble=false
f_mesh=false
f_hal=false
f_os=false
bsp_flag=false
bsp_valid=false
bsp_name="None"

display_help () {
  echo "Usage: build.sh [OPTION] -B BSP"
  echo "  -r    build Release variant"
  echo "  -d    build Debug variant"
  echo "  -l    enable LoRaWAN"
  echo "  -b    enable BLE"
  echo "  -m    enable LoRa mesh"
  echo "  -O    rebuild OS"
  echo "  -H    rebuild HAL"
  echo "  -B    valid BSP values:"
  for bsp in "${bsp_list[@]}"
  do
    echo "          $bsp"
  done
}

if [ $# -eq 0 ]; then
  display_help
  exit 1
fi

while getopts "rdlbmHOB:" flag; do
  case ${flag} in
    r)
      variant="Release"
      ;;
    d)
      variant="Debug"
      ;;
    l)
      f_lorawan=true
      ;;
    b)
      f_ble=true
      ;;
    m)
      f_mesh=true
      ;;
    O)
      f_os=true
      ;;
    H)
      f_hal=true
      ;;
    B)
      bsp_flag=true
      bsp_name=$OPTARG
      ;;
    ?)
      display_help
      exit 1
      ;;
  esac
done

if ( ! $bsp_flag ) then
  echo "Missing -B BSP specification."
  echo ""
  display_help
  exit 1
fi

for bsp in "${bsp_list[@]}"
do
  if [ "$bsp_name" = "$bsp" ]; then
    bsp_valid=true
  fi
done

if ( ! $bsp_valid ) then
  echo "Invalid BSP specified"
  echo ""
  display_help
  exit 1
fi

SCRIPT_DIR=$(cd $(dirname -- "${BASH_SOURCE[0]}") && pwd)
ROOT_DIR=$(cd ${SCRIPT_DIR}/../.. && pwd)
BUILD_DIR=${ROOT_DIR}/build

cd ${ROOT_DIR}

if [ -d ${BUILD_DIR} ]; then
  rm -rf ${BUILD_DIR}
fi

cmake \
  -DCMAKE_BUILD_TYPE=$variant \
  -D$bsp_name=1 \
  -DFEATURE_RAT_LORAWAN_ENABLE=$f_lorawan \
  -DFEATURE_RAT_BLE_ENABLE=$f_ble \
  -DFEATURE_RAT_LORA_MESH_ENABLE=$f_mesh \
  -DBUILD_LORAWAN=$f_lorawan \
  -DBUILD_BLE=$f_ble \
  -DBUILD_LORA_MESH=$f_mesh \
  -DBUILD_RTOS=$f_os \
  -DBUILD_HAL=$f_hal \
  -S . -B ${BUILD_DIR} -G "Unix Makefiles"
if [ $? -ne 0 ]; then
  echo "Configuration error"
  exit 1
fi
echo "$target_name $variant Config Completed"

cmake --build ${BUILD_DIR}
if [ $? -ne 0 ]; then
  echo "Build error"
  exit 1
fi
echo "$target_name $variant Build Completed"
