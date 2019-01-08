<?php

class RoomViewModel extends ViewModel {
    public $viewFields = array(
        'Room'=>array('*','_type'=>'LEFT'), //Must be LEFT 返回左表所有数据，即使右表没有匹配的
        'Subject'=>array('subject_id','subject_name','_on'=>'Subject.subject_id=Room.subject_id','_type'=>'LEFT'),
        'Image'=>array('image_id','file_fdfs'=>'image_fdfs','_on'=>'Image.image_id=Room.image_id','_type'=>'LEFT'),
        'Poster'=>array('file_fdfs'=>'poster_fdfs','_table'=>'wk_image','_on'=>'Poster.image_id=Room.poster_id','_type'=>'LEFT'),
        'User'=>array('user_id','real_name'=>'teacher_name','wx_nickname','wx_sex'=>'sex_name','_on'=>'Room.teacher_id=User.user_id'),
    );
}
?>