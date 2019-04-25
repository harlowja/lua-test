blah = 2.0

function stuff()
    return 2
end

-- This function must exist!
function build_configuration(vehicle, component)
    local w = {x=0, y=0, vehicle=vehicle, z=false, blah=blah, component=component}
    if vehicle == "josh" then
        w["b"] = "c"
    else
        w["b"] = "g"
    end
    w["s"] = stuff()
    w["g"] = {
        b=4
    }
    w["2pi"] = math.pi * 2
    w["days"] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"}
    return w
end
