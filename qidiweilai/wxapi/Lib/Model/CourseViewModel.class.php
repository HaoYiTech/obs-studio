<?php

class CourseViewModel extends ViewModel {
    public $viewFields = array(
        'Course'=>array('course_id','camera_id','repeat_id','week_id','elapse_sec','start_time','end_time','created','updated','_type'=>'LEFT'), //Must be LEFT 返回左表所有数据，即使右表没有匹配的
        'Camera'=>array('camera_id','gather_id','camera_name','_on'=>'Camera.camera_id=Course.camera_id'),
    );
}
?>