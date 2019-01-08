<?php

class RecordViewModel extends ViewModel {
    public $viewFields = array(
        'Record'=>array('*','_type'=>'LEFT'), //Must be LEFT 返回左表所有数据，即使右表没有匹配的
        'Camera'=>array('camera_id','camera_name','stream_prop','_on'=>'Camera.camera_id=Record.camera_id','_type'=>'LEFT'), //Must be LEFT
        'Image'=>array('image_id','file_fdfs'=>'image_fdfs','_on'=>'Image.image_id=Record.image_id'),
    );
}
?>